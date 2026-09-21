// Month-long soak simulation of the tester firmware logic (v2.5).
// Uses the REAL src/bms_protocol.cpp (parser + dispatcher + tracker) and
// the REAL src/relay_ctrl.cpp (sequencer + spoof plan), driven exactly like
// main.cpp's loop: feed poll bytes -> note_poll -> reply -> evaluate on a
// 250 ms grid; seq.tick() + spoof.stage() on a 100 ms grid.
// Time is uint32_t on purpose: starting at simulated day 40, the run CROSSES
// the 32-bit millis() wrap (~49.7 days) mid-month, exactly like a box left
// plugged in for weeks.
// Scenario: 30 days, 1 s polls, daily noise bursts, 5 s silence every 6 h,
// a looped sequential program all month (mode/count changes, forces, stops,
// weekly spoof fires), plus an NVS write-coalescing model mirroring
// web_ui's dirty-flag + 1.5 s flush rule (5-save bursts, 3x/day).
#include <cstdio>
#include <cstdint>
#include "bms_protocol.h"
#include "relay_ctrl.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); fails++; } } while (0)

int main() {
  JbdParser parser;
  PollTracker tracker;
  JbdFrame f;
  const uint8_t poll[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};

  Bms2Config cfg;
  cfg.step_delay_ms = 500;
  cfg.hold_seq_ms = 2000;
  cfg.relay_count = 8;
  cfg.relay_mode = RELAY_SEQUENTIAL;
  cfg.loop_enabled = true;
  cfg.cycle_pause_ms = 1000;
  cfg.cycle_limit = 0;
  RelaySequencer seq;
  seq.begin(&cfg);
  SpoofPlan spoof;

  // NVS coalescing model: mirrors web_ui (dirty + 1.5 s idle flush).
  bool nvs_dirty = false;
  uint32_t nvs_dirty_at = 0;
  unsigned long nvs_commits = 0;

  uint32_t now = 40u * 24u * 3600u * 1000u;  // simulated day 40
  const uint32_t day0 = now;
  const int STEPS = 30 * 24 * 3600;          // 30 days of 1 s polls
  unsigned long polls = 0, replies = 0, green_evals = 0, red_evals = 0;
  bool wrapped = false;
  unsigned long stop_events = 0;
  seq.start(now);

  for (int step = 0; step < STEPS; step++) {
    if (now < day0) wrapped = true;
    int day = step / (24 * 3600);
    bool silence_gap = (step % (6 * 3600) >= 6 * 3600 - 5);
    if (step % (24 * 3600) == 0) {  // daily 500-byte noise burst
      uint32_t s = 0xBEEF + (uint32_t)step;
      for (int i = 0; i < 500; i++) {
        s = s * 1664525u + 1013904223u;
        if (parser.feed((uint8_t)((s >> 16) & 0xFF), f)) {
          printf("FAIL: noise emitted frame at step %d\n", step);
          fails++;
        }
      }
      parser.reset();
    }
    if (!silence_gap) {
      int hits = 0;
      for (int i = 0; i < 7; i++)
        if (parser.feed(poll[i], f)) hits++;
      if (hits != 1) { printf("FAIL: hits=%d at step %d\n", hits, step); fails++; }
      else {
        polls++;
        tracker.note_poll(now);
        size_t rl = 0;
        const uint8_t *r = reply_for(f.reg, f.is_write, rl);
        if (r && rl == 34) replies++;
        else { printf("FAIL: no 0x03 reply at step %d\n", step); fails++; }
      }
    }
    for (int e = 0; e < 4; e++) {  // 250 ms eval grid
      if (tracker.active(now + (uint32_t)(e * 250))) green_evals++;
      else red_evals++;
    }
    // Operator script across the month (virtual bench tech).
    if (day == 10 && step % (24 * 3600) == 0) {
      cfg.relay_count = 4;  // shrink live: out-of-range drops same tick
      seq.countChanged();
    }
    if (day == 15 && step % (24 * 3600) == 0) {
      seq.stopAll(now);  // stop first (R10), settle, then switch to chase
      stop_events++;
    }
    if (day == 15 && step % (24 * 3600) == 36000) {  // 10 h later
      cfg.relay_mode = RELAY_CHASE;
      cfg.chase_sweeps = 0;
      seq.start(now);
    }
    if (day == 16 && step % (24 * 3600) == 0) {
      seq.setForced(2, true);  // force-in-chase: wave must idle (R7)
      if (seq.running() || seq.onCount() != 1) {
        printf("FAIL: force-in-chase not single at step %d\n", step);
        fails++;
      }
      seq.clearForced();
      seq.stopAll(now);
      stop_events++;
    }
    if (day == 16 && step % (24 * 3600) == 36000) {
      cfg.relay_mode = RELAY_SEQUENTIAL;
      cfg.relay_count = 8;
      seq.start(now);
    }
    if ((day == 7 || day == 14 || day == 21 || day == 28) &&
        step % (24 * 3600) == 0) {
      spoof.trigger(now, 5000, 10000);  // weekly 5 s + 10 s fire
    }
    if (step % (24 * 3600) == 43200) {  // midday save bursts
      for (int k = 0; k < 5; k++) { nvs_dirty = true; nvs_dirty_at = now; }
    }
    // 100 ms relay + spoof grid inside each simulated second.
    for (int e = 0; e < 10; e++) {
      uint32_t t = now + (uint32_t)(e * 100);
      seq.tick(t);
      if (seq.onCount() > 8) {
        printf("FAIL: too many ON at step %d\n", step);
        fails++;
        break;
      }
      if (spoof.stage(t) > 2) {
        printf("FAIL: bad spoof stage at step %d\n", step);
        fails++;
      }
    }
    if (nvs_dirty && (now - nvs_dirty_at) >= 1500u) {
      nvs_dirty = false;
      nvs_commits++;
    }
    now += 1000;
  }

  unsigned long cyc = seq.cyclesDone();
  unsigned long acts = seq.actuations();
  printf("days=30 polls=%lu replies=%lu green=%lu red=%lu wrapped=%d\n",
         polls, replies, green_evals, red_evals, (int)wrapped);
  printf("cycles=%lu actuations=%lu stops=%lu nvs_commits=%lu\n", cyc, acts,
         stop_events, nvs_commits);
  CHECK(wrapped);                    // rollover really crossed mid-month
  CHECK(polls == replies);           // every poll answered, none lost
  CHECK(polls > 2500000);
  CHECK(green_evals > 0 && red_evals > 0);
  CHECK(cyc > 100000);               // loop ran all month
  CHECK(stop_events == 2);
  CHECK(nvs_commits <= 30 * 4);      // coalesced: ~1/day bursts, not per-save
  CHECK(!tracker.active(now + 20000u));   // silence -> red, no reset
  for (int i = 0; i < 7; i++) parser.feed(poll[i], f);
  tracker.note_poll(now + 21000u);        // polls resume after a month...
  CHECK(tracker.active(now + 21500u));    // ...green again, no reset

  printf(fails ? "SOAK FAIL\n" : "SOAK PASS: 30-day run, rollover crossed, no reset needed\n");
  return fails;
}
