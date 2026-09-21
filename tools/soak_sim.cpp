// Week-long soak simulation of the tester firmware logic (v1.2).
// Uses the REAL src/bms_protocol.cpp (parser + dispatcher + tracker),
// driven exactly like main.cpp's loop: feed poll bytes -> note_poll ->
// reply via reply_for -> evaluate tracker.active() on a 250 ms grid.
// Time is uint32_t on purpose: the sim CROSSES the 32-bit millis() wrap
// (~49.7 days) mid-run, exactly like a device left plugged in.
// Scenario: 8 days, 1 s polls, daily noise bursts, 5 s silence every 6 h.
#include <cstdio>
#include <cstdint>
#include "bms_protocol.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); fails++; } } while (0)

int main() {
  JbdParser parser;
  PollTracker tracker;
  JbdFrame f;
  const uint8_t poll[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};

  uint32_t now = 0xFFFFFFFFu - 100000u;  // 100 s before millis() wrap
  const int STEPS = 8 * 24 * 3600;       // 8 days of 1 s polls
  unsigned long polls = 0, replies = 0, green_evals = 0, red_evals = 0;
  bool wrapped = false;

  for (int step = 0; step < STEPS; step++) {
    if (now < 100000u) wrapped = true;
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
    now += 1000;
  }

  printf("days=8 polls=%lu replies=%lu green=%lu red=%lu wrapped=%d\n",
         polls, replies, green_evals, red_evals, (int)wrapped);
  CHECK(wrapped);                    // rollover really crossed mid-run
  CHECK(polls == replies);           // every poll answered, none lost
  CHECK(polls > 690000);
  CHECK(green_evals > 0 && red_evals > 0);
  CHECK(!tracker.active(now + 20000u));   // silence -> red, no reset
  for (int i = 0; i < 7; i++) parser.feed(poll[i], f);
  tracker.note_poll(now + 21000u);        // polls resume after a week off...
  CHECK(tracker.active(now + 21500u));    // ...green again, no reset

  printf(fails ? "SOAK FAIL\n" : "SOAK PASS: 8-day run, rollover crossed, no reset needed\n");
  return fails;
}
