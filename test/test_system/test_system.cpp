// System reliability: select_reply() matrix + a 24 h simulated office day
// driving parser + sequencer + spoof + tracker exactly like main.cpp's loop.
// Every reply on the virtual bus is checksum-validated; relay schedule,
// spoof window and link state are asserted throughout.
#include <unity.h>
#include <vector>
#include "../../src/relay_ctrl.h"
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

static JbdFrame mk_frame(uint8_t reg, bool is_write) {
  JbdFrame f;
  f.is_write = is_write;
  f.reg = reg;
  f.len = 0;
  return f;
}

// Validate a raw JBD response: DD REG LH LL DATA CK 77 + checksum rule.
static bool valid_response(const uint8_t *b, size_t n) {
  if (n < 7 || b[0] != 0xDD || b[n - 1] != 0x77) return false;
  size_t dl = ((size_t)b[2] << 8) | b[3];
  if (n != 7 + dl) return false;
  uint32_t s = 0;
  for (size_t i = 2; i < 4 + dl; i++) s += b[i];
  uint16_t want = (uint16_t)(0x10000UL - (s & 0xFFFFUL));
  return b[4 + dl] == (want >> 8) && b[5 + dl] == (want & 0xFF);
}

void test_select_matrix(void) {
  Bms2Config c;
  uint8_t sf[34];
  build_spoof_frame(c, sf);
  size_t rl = 0;
  const uint8_t *r;

  r = select_reply(mk_frame(0x03, false), c, false, sf, rl);
  TEST_ASSERT_EQUAL_PTR(BMS_RESPONSE, r);
  TEST_ASSERT_EQUAL_UINT(34, rl);

  r = select_reply(mk_frame(0x03, false), c, true, sf, rl);
  TEST_ASSERT_EQUAL_PTR(sf, r);
  TEST_ASSERT_EQUAL_UINT(34, rl);
  TEST_ASSERT_TRUE(valid_response(r, rl));

  c.spoof_enabled = false;
  r = select_reply(mk_frame(0x03, false), c, true, sf, rl);
  TEST_ASSERT_EQUAL_PTR(BMS_RESPONSE, r);
  c.spoof_enabled = true;

  r = select_reply(mk_frame(0x03, false), c, true, nullptr, rl);
  TEST_ASSERT_EQUAL_PTR(BMS_RESPONSE, r);  // no frame built yet -> golden

  r = select_reply(mk_frame(0x04, false), c, true, sf, rl);
  TEST_ASSERT_EQUAL_PTR(BMS_RESPONSE_CELLS, r);
  r = select_reply(mk_frame(0x05, false), c, true, sf, rl);
  TEST_ASSERT_EQUAL_PTR(BMS_RESPONSE_NAME, r);

  r = select_reply(mk_frame(0x03, true), c, true, sf, rl);
  TEST_ASSERT_NULL(r);  // writes silent even mid-spoof
  r = select_reply(mk_frame(0x09, false), c, true, sf, rl);
  TEST_ASSERT_NULL(r);  // unknown silent even mid-spoof
}

void test_office_day_24h(void) {
  Bms2Config c;
  c.step_delay_ms = 500;
  c.hold_seconds = 120;
  c.relay_mode = RELAY_SEQUENTIAL;
  c.button_mode = BTN_HOLD_ABORT;
  uint8_t sf[34];
  build_spoof_frame(c, sf);

  JbdParser parser;
  PollTracker tracker;
  RelaySequencer seq;
  SpoofWindow spoof;
  seq.begin(&c);
  const uint8_t poll03[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  const uint8_t poll04[7] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
  // Valid JBD write: DD 5A 03 02 AA 55 CK 77, CK = 0x10000-(03+02+AA+55).
  const uint8_t write03[9] = {0xDD, 0x5A, 0x03, 0x02, 0xAA, 0x55,
                              0xFE, 0xFC, 0x77};

  unsigned long polls = 0, replies = 0, spoof_replies = 0, golden03 = 0;
  unsigned long green_evals = 0, red_evals = 0;
  bool saw_r1_at_8 = false, saw_r8_at_8 = false, saw_off_after_abort = false;
  bool saw_off_after_hold = false, allon_at_18 = false;
  bool write_got_reply = false;
  JbdFrame f;

  const unsigned long DAY = 24UL * 3600UL * 1000UL;
  const unsigned long T8 = 8UL * 3600UL * 1000UL;
  const unsigned long T12 = 12UL * 3600UL * 1000UL;
  const unsigned long T18 = 18UL * 3600UL * 1000UL;
  unsigned long noise_seed = 0xBEEF;

  for (unsigned long now = 0; now < DAY; now += 100) {
    // Events.
    if (now == T8) handle_button_press(seq, c, now);            // start seq
    if (now == T8 + 60000UL) handle_button_press(seq, c, now);  // abort
    if (now == T8 + 3600000UL) handle_button_press(seq, c, now);  // restart
    if (now == T12) spoof.trigger(now, 10000);                  // web FIRE
    if (now == T18) {
      c.relay_mode = RELAY_ALL_ON;
      seq.start(now);
    }
    // Hourly noise burst (must never emit).
    if (now % 3600000UL == 0 && now > 0) {
      for (int i = 0; i < 100; i++) {
        noise_seed = noise_seed * 1664525u + 1013904223u;
        if (parser.feed((uint8_t)(noise_seed >> 16), f)) {
          TEST_FAIL_MESSAGE("noise emitted a frame");
          return;
        }
      }
      parser.reset();
    }
    // 1 s meter poll (03; 04 interleaved hourly at :30).
    if (now % 1000UL == 0) {
      const uint8_t *poll = poll03;
      size_t pl = 7;
      bool is04 = (now % 3600000UL == 1800000UL);
      if (is04) {
        poll = poll04;
      }
      (void)pl;
      int hits = 0;
      for (int i = 0; i < 7; i++)
        if (parser.feed(poll[i], f)) hits++;
      if (hits != 1) {
        TEST_FAIL_MESSAGE("poll not emitted exactly once");
        return;
      }
      polls++;
      tracker.note_poll(now);
      size_t rl = 0;
      const uint8_t *reply =
          select_reply(f, c, spoof.active(now), sf, rl);
      if (!reply) {
        TEST_FAIL_MESSAGE("read got silence");
        return;
      }
      if (!valid_response(reply, rl)) {
        TEST_FAIL_MESSAGE("reply failed checksum validation");
        return;
      }
      replies++;
      if (f.reg == 0x03) {
        if (spoof.active(now)) {
          spoof_replies++;
          if (reply != sf) {
            TEST_FAIL_MESSAGE("spoof window did not serve spoof frame");
            return;
          }
        } else {
          golden03++;
          if (reply != BMS_RESPONSE) {
            TEST_FAIL_MESSAGE("non-spoof 03 not golden");
            return;
          }
        }
      }
      // Malicious write mid-spoof must stay silent.
      if (now == T12 + 5000UL) {
        int whits = 0;
        for (int i = 0; i < 9; i++)
          if (parser.feed(write03[i], f)) whits++;
        if (whits == 1) {
          size_t wrl = 0;
          if (select_reply(f, c, spoof.active(now), sf, wrl))
            write_got_reply = true;
        }
      }
    }
    seq.tick(now);
    // Schedule probes.
    if (now == T8 + 100UL) saw_r1_at_8 = seq.relayOn(0) && seq.onCount() == 1;
    if (now == T8 + 3600UL)
      saw_r8_at_8 = seq.onCount() == 8;  // 7 steps x 500 ms + R1
    if (now == T8 + 60100UL)
      saw_off_after_abort = !seq.running() && seq.onCount() == 0;
    if (now == T8 + 3600000UL + 124000UL)
      saw_off_after_hold = !seq.running() && seq.onCount() == 0;
    if (now == T18 + 100UL) allon_at_18 = seq.onCount() == 8;
    // 250 ms LED grid.
    if (now % 250UL == 0) {
      if (tracker.active(now))
        green_evals++;
      else
        red_evals++;
    }
  }

  TEST_ASSERT_EQUAL_UINT(86400, polls);  // one poll per second, all day
  TEST_ASSERT_EQUAL_UINT(polls, replies);
  TEST_ASSERT_EQUAL_UINT(10, spoof_replies);  // exactly the 10 s window
  TEST_ASSERT_FALSE(write_got_reply);
  TEST_ASSERT_TRUE(saw_r1_at_8);
  TEST_ASSERT_TRUE(saw_r8_at_8);
  TEST_ASSERT_TRUE(saw_off_after_abort);
  TEST_ASSERT_TRUE(saw_off_after_hold);
  TEST_ASSERT_TRUE(allon_at_18);
  TEST_ASSERT_TRUE(green_evals > red_evals * 100);  // green all day
  TEST_ASSERT_TRUE(golden03 > 86000);
}

void run_all() {
  RUN_TEST(test_select_matrix);
  RUN_TEST(test_office_day_24h);
}

int main() {
  UNITY_BEGIN();
  run_all();
  return UNITY_END();
}
