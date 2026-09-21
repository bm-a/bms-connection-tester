#include <unity.h>
#include "../../src/relay_ctrl.h"
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// v2.0 spoof: default 88.8/88.8/88.8/188 frame bytes, checksum validity,
// custom values, window timing/expiry/cancel, rollover safety.

void test_spoof_default_field_bytes(void) {
  Bms2Config c;  // defaults: 888/888/888/188, 10 s
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, f);
  TEST_ASSERT_EQUAL_UINT8(0xDD, f[0]);
  TEST_ASSERT_EQUAL_UINT8(0x03, f[1]);
  TEST_ASSERT_EQUAL_UINT8(0x00, f[2]);
  TEST_ASSERT_EQUAL_UINT8(0x1B, f[3]);
  TEST_ASSERT_EQUAL_UINT8(0x22, f[4]);   // 8880 = 88.80 V
  TEST_ASSERT_EQUAL_UINT8(0xB0, f[5]);
  TEST_ASSERT_EQUAL_UINT8(0x22, f[6]);   // 8880 = 88.80 A
  TEST_ASSERT_EQUAL_UINT8(0xB0, f[7]);
  TEST_ASSERT_EQUAL_UINT8(188, f[23]);   // 0xBC out-of-range pattern
  TEST_ASSERT_EQUAL_UINT8(0x0E, f[27]);  // 3619 = 2731+888 -> 88.8 C
  TEST_ASSERT_EQUAL_UINT8(0x23, f[28]);
  TEST_ASSERT_EQUAL_UINT8(0x0E, f[29]);
  TEST_ASSERT_EQUAL_UINT8(0x23, f[30]);
  TEST_ASSERT_EQUAL_UINT8(0x77, f[33]);
  // Untouched golden context: capacities, cells, version, FET.
  TEST_ASSERT_EQUAL_UINT8(0x27, f[8]);
  TEST_ASSERT_EQUAL_UINT8(0x10, f[9]);
  TEST_ASSERT_EQUAL_UINT8(0x0E, f[25]);  // 14S cell count
}

void test_spoof_checksum_valid(void) {
  Bms2Config c;
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, f);
  // Same rule as the captures: 0x10000 - sum([2..30]).
  uint16_t want = jbd_checksum(&f[2], 29);
  uint16_t got = ((uint16_t)f[31] << 8) | f[32];
  TEST_ASSERT_EQUAL_UINT16(want, got);
  // And the strict host parser must ACCEPT the spoof frame bytes as a
  // well-formed reply-shaped... (requests only: feed the golden request to
  // prove the builder didn't disturb shared state).
  JbdParser p;
  JbdFrame fr;
  const uint8_t poll[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  int hits = 0;
  for (int i = 0; i < 7; i++)
    if (p.feed(poll[i], fr)) hits++;
  TEST_ASSERT_EQUAL_INT(1, hits);
  TEST_ASSERT_EQUAL_UINT8(0x03, fr.reg);
}

void test_spoof_custom_values(void) {
  Bms2Config c;
  c.spoof_v_tenth = 600;   // 60.0 V -> 6000 = 0x1770
  c.spoof_a_tenth = 123;   // 12.3 A -> 1230 = 0x04CE
  c.spoof_c_tenth = 250;   // 25.0 C -> 2981 = 0x0BA5
  c.spoof_soc = 50;
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, f);
  TEST_ASSERT_EQUAL_UINT8(0x17, f[4]);
  TEST_ASSERT_EQUAL_UINT8(0x70, f[5]);
  TEST_ASSERT_EQUAL_UINT8(0x04, f[6]);
  TEST_ASSERT_EQUAL_UINT8(0xCE, f[7]);
  TEST_ASSERT_EQUAL_UINT8(50, f[23]);
  TEST_ASSERT_EQUAL_UINT8(0x0B, f[27]);
  TEST_ASSERT_EQUAL_UINT8(0xA5, f[28]);
  uint16_t want = jbd_checksum(&f[2], 29);
  TEST_ASSERT_EQUAL_UINT16(want, ((uint16_t)f[31] << 8) | f[32]);
}

void test_spoof_window_timing(void) {
  SpoofWindow w;
  TEST_ASSERT_FALSE(w.active(10000));
  w.trigger(10000, 10000);  // 10 s
  TEST_ASSERT_TRUE(w.active(10000));
  TEST_ASSERT_TRUE(w.active(19999));
  TEST_ASSERT_FALSE(w.active(20000));  // auto-revert boundary
  TEST_ASSERT_FALSE(w.active(30000));
}

void test_spoof_window_cancel_retrigger(void) {
  SpoofWindow w;
  w.trigger(10000, 10000);
  w.cancel();
  TEST_ASSERT_FALSE(w.active(10001));
  w.trigger(10001, 5000);
  TEST_ASSERT_TRUE(w.active(14000));
  w.trigger(14000, 5000);  // retrigger extends
  TEST_ASSERT_TRUE(w.active(18000));
  TEST_ASSERT_FALSE(w.active(19000));
}

void test_spoof_window_rollover(void) {
  SpoofWindow w;
  unsigned long t0 = 0xFFFFFFFFu - 2000u;
  w.trigger(t0, 5000);  // expires 3000 after wrap
  TEST_ASSERT_TRUE(w.active(t0 + 4000));
  TEST_ASSERT_FALSE(w.active(t0 + 6000));
}

void run_all() {
  RUN_TEST(test_spoof_default_field_bytes);
  RUN_TEST(test_spoof_checksum_valid);
  RUN_TEST(test_spoof_custom_values);
  RUN_TEST(test_spoof_window_timing);
  RUN_TEST(test_spoof_window_cancel_retrigger);
  RUN_TEST(test_spoof_window_rollover);
}

#ifdef ARDUINO
#include <Arduino.h>
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
