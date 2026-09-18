#include <unity.h>
#include <cstring>
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// Feed a whole frame through the parser; expect exactly one valid emit.
static bool parse_once(const uint8_t *frame, size_t len, JbdFrame &out) {
  JbdParser p;
  int hits = 0;
  bool ok = false;
  for (size_t i = 0; i < len; i++) {
    if (p.feed(frame[i], out)) { hits++; ok = true; }
  }
  return ok && hits == 1;
}

void test_parser_03_read(void) {
  uint8_t req[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  JbdFrame f;
  TEST_ASSERT_TRUE(parse_once(req, 7, f));
  TEST_ASSERT_FALSE(f.is_write);
  TEST_ASSERT_EQUAL_UINT8(0x03, f.reg);
  TEST_ASSERT_EQUAL_UINT8(0, f.len);
}

void test_parser_04_05_reads(void) {
  // DD A5 04 00 FFFC 77 / DD A5 05 00 FFFB 77
  uint8_t r4[7] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
  uint8_t r5[7] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};
  JbdFrame f;
  TEST_ASSERT_TRUE(parse_once(r4, 7, f));
  TEST_ASSERT_EQUAL_UINT8(0x04, f.reg);
  TEST_ASSERT_FALSE(f.is_write);
  TEST_ASSERT_TRUE(parse_once(r5, 7, f));
  TEST_ASSERT_EQUAL_UINT8(0x05, f.reg);
}

void test_parser_write_flagged(void) {
  // Write reg 0x10, 2 data bytes: cover = 10 02 AA 55 -> sum=0x111 -> ck=FEEF
  uint8_t w[10] = {0xDD, 0x5A, 0x10, 0x02, 0xAA, 0x55, 0xFE, 0xEF, 0x77, 0x00};
  JbdFrame f;
  TEST_ASSERT_TRUE(parse_once(w, 9, f));
  TEST_ASSERT_TRUE(f.is_write);
  TEST_ASSERT_EQUAL_UINT8(0x10, f.reg);
  TEST_ASSERT_EQUAL_UINT8(2, f.len);
  TEST_ASSERT_EQUAL_UINT8(0xAA, f.data[0]);
  TEST_ASSERT_EQUAL_UINT8(0x55, f.data[1]);
}

void test_parser_rejects_corrupt(void) {
  uint8_t good[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  for (int i = 0; i < 7; i++) {
    uint8_t bad[7];
    memcpy(bad, good, 7);
    bad[i] ^= 0x01;
    JbdFrame f;
    JbdParser p;
    bool hit = false;
    for (int j = 0; j < 7; j++) hit |= p.feed(bad[j], f);
    TEST_ASSERT_FALSE(hit);
  }
}

void test_parser_resync_on_noise(void) {
  // Garbage incl. stray DDs, then a real frame: must emit exactly once.
  uint8_t stream[] = {0x00, 0xDD, 0xDD, 0xA5, 0xFF, 0xDD,
                      0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  JbdFrame f;
  JbdParser p;
  int hits = 0;
  for (size_t i = 0; i < sizeof(stream); i++) {
    if (p.feed(stream[i], f)) { hits++; TEST_ASSERT_EQUAL_UINT8(0x03, f.reg); }
  }
  TEST_ASSERT_EQUAL_INT(1, hits);
}

void test_parser_overlong_rejected(void) {
  // LEN=65 > JBD_MAX_DATA: rejected even with trailing 0x77.
  uint8_t s[6] = {0xDD, 0xA5, 0x09, 65, 0x77, 0x77};
  JbdFrame f;
  JbdParser p;
  bool hit = false;
  for (int i = 0; i < 6; i++) hit |= p.feed(s[i], f);
  TEST_ASSERT_FALSE(hit);
}

void test_parser_split_delivery(void) {
  // Same frame fed one byte per "tick": still exactly one emit.
  uint8_t req[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  JbdFrame f;
  JbdParser p;
  int hits = 0;
  for (int i = 0; i < 7; i++) {
    if (p.feed(req[i], f)) hits++;
  }
  TEST_ASSERT_EQUAL_INT(1, hits);
}

void test_dispatcher_answers_known_reads(void) {
  size_t n = 0;
  const uint8_t *r;
  r = reply_for(0x03, false, n);
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT(34, n);
  TEST_ASSERT_EQUAL_UINT8(0xDD, r[0]);
  TEST_ASSERT_EQUAL_UINT8(0x77, r[n - 1]);
  r = reply_for(0x04, false, n);
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT(35, n);
  r = reply_for(0x05, false, n);
  TEST_ASSERT_NOT_NULL(r);
  TEST_ASSERT_EQUAL_UINT(19, n);
}

void test_dispatcher_silent_option_a(void) {
  size_t n = 0xBEEF;
  TEST_ASSERT_NULL(reply_for(0x03, true, n));   // write: silence
  TEST_ASSERT_NULL(reply_for(0x04, true, n));   // write: silence
  TEST_ASSERT_NULL(reply_for(0x09, false, n));  // unknown reg: silence
  TEST_ASSERT_NULL(reply_for(0xFF, false, n));  // unknown reg: silence
  TEST_ASSERT_NULL(reply_for(0x10, true, n));   // write: silence
}

void test_canned_frames_self_consistent(void) {
  // Each canned reply's embedded checksum must equal jbd_checksum over
  // LEN_HI+LEN_LO+DATA (same verified rule as the real captures).
  TEST_ASSERT_EQUAL_HEX16(0xFCDA, jbd_checksum(BMS_RESPONSE + 2, 2 + 27));
  TEST_ASSERT_EQUAL_HEX16(0xF804, jbd_checksum(BMS_RESPONSE_CELLS + 2, 2 + 28));
  TEST_ASSERT_EQUAL_HEX16(0xFCFD, jbd_checksum(BMS_RESPONSE_NAME + 2, 2 + 12));
  // And the checksum bytes sit where claimed:
  TEST_ASSERT_EQUAL_HEX8(0xFC, BMS_RESPONSE[31]);
  TEST_ASSERT_EQUAL_HEX8(0xDA, BMS_RESPONSE[32]);
  TEST_ASSERT_EQUAL_HEX8(0xF8, BMS_RESPONSE_CELLS[32]);
  TEST_ASSERT_EQUAL_HEX8(0x04, BMS_RESPONSE_CELLS[33]);
  TEST_ASSERT_EQUAL_HEX8(0xFC, BMS_RESPONSE_NAME[16]);
  TEST_ASSERT_EQUAL_HEX8(0xFD, BMS_RESPONSE_NAME[17]);
}

void test_fuzz_no_false_frames(void) {
  // Deterministic LCG noise, 1M bytes: strict parser must not emit.
  // (A valid frame needs DD + valid cmd + len + exact checksum + 77.)
  JbdParser p;
  JbdFrame f;
  uint32_t s = 0x12345678;
  int hits = 0;
  for (int i = 0; i < 1000000; i++) {
    s = s * 1664525 + 1013904223;
    uint8_t b = (s >> 16) & 0xFF;
    if (p.feed(b, f)) hits++;
  }
  TEST_ASSERT_EQUAL_INT(0, hits);
}

void test_soak_fast_poll(void) {
  // 100k polls at 100 ms cadence: every 250 ms eval stays green.
  PollTracker t;
  unsigned long now = 1000;
  for (int i = 0; i < 100000; i++) {
    t.note_poll(now);
    TEST_ASSERT_TRUE(t.active(now + 250));
    now += 100;
  }
}

void test_soak_slow_poll(void) {
  // 5 s cadence: adaptive window (10 s) keeps green; 11 s gap goes red.
  PollTracker t;
  unsigned long now = 1000;
  for (int i = 0; i < 200; i++) {
    t.note_poll(now);
    TEST_ASSERT_TRUE(t.active(now + 1000));
    now += 5000;
  }
  TEST_ASSERT_FALSE(t.active(now + 11000));
}

void run_all() {
  RUN_TEST(test_parser_03_read);
  RUN_TEST(test_parser_04_05_reads);
  RUN_TEST(test_parser_write_flagged);
  RUN_TEST(test_parser_rejects_corrupt);
  RUN_TEST(test_parser_resync_on_noise);
  RUN_TEST(test_parser_overlong_rejected);
  RUN_TEST(test_parser_split_delivery);
  RUN_TEST(test_dispatcher_answers_known_reads);
  RUN_TEST(test_dispatcher_silent_option_a);
  RUN_TEST(test_canned_frames_self_consistent);
  RUN_TEST(test_fuzz_no_false_frames);
  RUN_TEST(test_soak_fast_poll);
  RUN_TEST(test_soak_slow_poll);
}

#ifdef ARDUINO
void setup() {
  delay(1000);
  UNITY_BEGIN();
  run_all();
  UNITY_END();
}
void loop() {}
#else
int main() {
  UNITY_BEGIN();
  run_all();
  return UNITY_END();
}
#endif
