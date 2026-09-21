#include <unity.h>
#include "../../src/relay_ctrl.h"
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// v2.3 spoof: stage 1 ("100" first) + stage 2 (88.8/88.8/88.8/188 pattern)
// frame bytes, checksum validity, custom values, plan timing/cancel,
// window (legacy, frozen) timing/expiry/cancel, rollover safety.

void test_spoof_stage2_default_field_bytes(void) {
  Bms2Config c;  // stage 2 defaults: 888/888/888/188, 10 s
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, 2, f);
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

void test_spoof_stage1_default_field_bytes(void) {
  Bms2Config c;  // stage 1 defaults: 100.0/100.0/100.0/100 %, 5 s
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, 1, f);
  TEST_ASSERT_EQUAL_UINT8(0xDD, f[0]);
  TEST_ASSERT_EQUAL_UINT8(0x03, f[1]);
  TEST_ASSERT_EQUAL_UINT8(0x27, f[4]);   // 10000 = 100.00 V
  TEST_ASSERT_EQUAL_UINT8(0x10, f[5]);
  TEST_ASSERT_EQUAL_UINT8(0x27, f[6]);   // 10000 = 100.00 A
  TEST_ASSERT_EQUAL_UINT8(0x10, f[7]);
  TEST_ASSERT_EQUAL_UINT8(100, f[23]);   // realistic full pack
  TEST_ASSERT_EQUAL_UINT8(0x0E, f[27]);  // 3731 = 2731+1000 -> 100.0 C
  TEST_ASSERT_EQUAL_UINT8(0x93, f[28]);
  TEST_ASSERT_EQUAL_UINT8(0x77, f[33]);
  uint16_t want = jbd_checksum(&f[2], 29);
  TEST_ASSERT_EQUAL_UINT16(want, ((uint16_t)f[31] << 8) | f[32]);
}

void test_spoof_checksum_valid(void) {
  Bms2Config c;
  uint8_t f[SPOOF_FRAME_LEN];
  build_spoof_frame(c, 2, f);
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
  build_spoof_frame(c, 1, f);
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

void test_spoof_plan_stages(void) {
  SpoofPlan p;
  TEST_ASSERT_EQUAL_UINT8(0, p.stage(10000));
  TEST_ASSERT_FALSE(p.active(10000));
  p.trigger(10000, 5000, 10000);  // stage 1: 5 s, stage 2: 10 s
  TEST_ASSERT_EQUAL_UINT8(1, p.stage(10000));
  TEST_ASSERT_EQUAL_UINT8(1, p.stage(14999));
  TEST_ASSERT_EQUAL_UINT8(2, p.stage(15000));  // exact handoff, no gap
  TEST_ASSERT_EQUAL_UINT8(2, p.stage(24999));
  TEST_ASSERT_EQUAL_UINT8(0, p.stage(25000));  // auto-revert boundary
  TEST_ASSERT_TRUE(p.active(20000));
  TEST_ASSERT_FALSE(p.active(25000));
}

void test_spoof_plan_cancel_retrigger(void) {
  SpoofPlan p;
  p.trigger(10000, 5000, 10000);
  p.cancel();
  TEST_ASSERT_EQUAL_UINT8(0, p.stage(10001));
  p.trigger(10001, 2000, 3000);
  TEST_ASSERT_EQUAL_UINT8(1, p.stage(11000));
  TEST_ASSERT_EQUAL_UINT8(2, p.stage(13000));
  p.trigger(13000, 2000, 3000);  // retrigger restarts at stage 1
  TEST_ASSERT_EQUAL_UINT8(1, p.stage(14000));
  TEST_ASSERT_EQUAL_UINT8(0, p.stage(19000));
}

void test_spoof_plan_rollover(void) {
  SpoofPlan p;
  unsigned long t0 = 0xFFFFFFFFu - 2000u;
  p.trigger(t0, 3000, 4000);  // stage 2 straddles the wrap
  TEST_ASSERT_EQUAL_UINT8(1, p.stage(t0 + 2000));
  TEST_ASSERT_EQUAL_UINT8(2, p.stage(t0 + 4000));
  TEST_ASSERT_EQUAL_UINT8(0, p.stage(t0 + 8000));
}

void test_spoof_stages_differ(void) {
  Bms2Config c;  // defaults must differ: 100s vs 88.8/188 pattern
  uint8_t a[SPOOF_FRAME_LEN], b[SPOOF_FRAME_LEN];
  build_spoof_frame(c, 1, a);
  build_spoof_frame(c, 2, b);
  TEST_ASSERT_TRUE(a[4] != b[4] || a[23] != b[23]);  // V and SOC differ
  TEST_ASSERT_EQUAL_UINT8(100, a[23]);
  TEST_ASSERT_EQUAL_UINT8(188, b[23]);
}

void run_all() {
  RUN_TEST(test_spoof_stage2_default_field_bytes);
  RUN_TEST(test_spoof_stage1_default_field_bytes);
  RUN_TEST(test_spoof_checksum_valid);
  RUN_TEST(test_spoof_custom_values);
  RUN_TEST(test_spoof_window_timing);
  RUN_TEST(test_spoof_window_cancel_retrigger);
  RUN_TEST(test_spoof_window_rollover);
  RUN_TEST(test_spoof_plan_stages);
  RUN_TEST(test_spoof_plan_cancel_retrigger);
  RUN_TEST(test_spoof_plan_rollover);
  RUN_TEST(test_spoof_stages_differ);
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
