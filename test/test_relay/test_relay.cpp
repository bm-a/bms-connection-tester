#include <unity.h>
#include "../../src/relay_ctrl.h"

void setUp(void) {}
void tearDown(void) {}

// v2.0 sequencer: boot OFF, sequential stepping, ALL-ON, 3 button modes,
// hold expiry, manual override, polarity helper, rollover safety.

static Bms2Config def_cfg() {
  Bms2Config c;
  c.step_delay_ms = 500;
  c.hold_seconds = 30;
  c.relay_mode = RELAY_SEQUENTIAL;
  c.button_mode = BTN_HOLD_ABORT;
  c.active_low = true;
  return c;
}

void test_seq_boots_all_off(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  for (uint8_t i = 0; i < RELAY_COUNT; i++) TEST_ASSERT_FALSE(s.relayOn(i));
}

void test_seq_sequential_steps(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_FALSE(s.relayOn(1));
  s.tick(10499);  // before first step
  TEST_ASSERT_FALSE(s.relayOn(1));
  s.tick(10500);  // R2 on
  TEST_ASSERT_TRUE(s.relayOn(1));
  TEST_ASSERT_FALSE(s.relayOn(2));
  // Fast-forward: all 8 on by 10000 + 7*500 = 13500.
  s.tick(13500);
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());
  TEST_ASSERT_TRUE(s.running());  // now holding
}

void test_seq_hold_expiry_auto_off(void) {
  Bms2Config c = def_cfg();
  c.hold_seconds = 2;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);  // all on, hold until 15500
  TEST_ASSERT_TRUE(s.running());
  s.tick(15499);
  TEST_ASSERT_TRUE(s.running());
  s.tick(15500);
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_seq_hold_forever_when_zero(void) {
  Bms2Config c = def_cfg();
  c.hold_seconds = 0;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);
  s.tick(1000000);  // far future, still on
  TEST_ASSERT_TRUE(s.running());
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());
}

void test_seq_all_on_mode(void) {
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_ALL_ON;
  c.hold_seconds = 5;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());  // immediate, no stepping
  s.tick(15000);
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_btn_hold_abort_mid_cycle(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  handle_button_press(s, c, 10000);  // start
  TEST_ASSERT_TRUE(s.running());
  s.tick(11000);
  handle_button_press(s, c, 11000);  // re-press -> OFF now
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_btn_run_lock_ignores(void) {
  Bms2Config c = def_cfg();
  c.button_mode = BTN_RUN_LOCK;
  RelaySequencer s;
  s.begin(&c);
  handle_button_press(s, c, 10000);
  s.tick(11000);
  TEST_ASSERT_TRUE(s.relayOn(0));
  handle_button_press(s, c, 11000);  // ignored mid-cycle
  TEST_ASSERT_TRUE(s.running());
  s.tick(11500);
  TEST_ASSERT_TRUE(s.relayOn(3));  // cycle continued, not restarted
}

void test_btn_restart_mid_cycle(void) {
  Bms2Config c = def_cfg();
  c.button_mode = BTN_RESTART;
  RelaySequencer s;
  s.begin(&c);
  handle_button_press(s, c, 10000);
  s.tick(13000);  // 7 relays on
  TEST_ASSERT_EQUAL_UINT8(7, s.onCount());
  handle_button_press(s, c, 13000);  // restart from R1
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_FALSE(s.relayOn(1));
}

void test_seq_manual_override(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  s.setForced(7, true);
  TEST_ASSERT_TRUE(s.relayOn(7));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  s.start(10000);  // sequence runs alongside forced relay
  TEST_ASSERT_TRUE(s.relayOn(7));
  s.stopAll();  // clears forces too
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_polarity_helper(void) {
  TEST_ASSERT_EQUAL_UINT8(0, relay_pin_level(true, true));   // active-low ON = LOW
  TEST_ASSERT_EQUAL_UINT8(1, relay_pin_level(false, true));  // active-low OFF = HIGH
  TEST_ASSERT_EQUAL_UINT8(1, relay_pin_level(true, false));  // active-high ON = HIGH
  TEST_ASSERT_EQUAL_UINT8(0, relay_pin_level(false, false));
}

void test_seq_rollover_safe(void) {
  Bms2Config c = def_cfg();
  c.hold_seconds = 2;
  RelaySequencer s;
  s.begin(&c);
  unsigned long t0 = 0xFFFFFFFFu - 1000u;  // 1 s before millis() wrap
  s.start(t0);
  s.tick(t0 + 4500);  // steps done across the wrap (last step t0+4000)
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());
  s.tick(t0 + 7000);  // hold (2 s) expired across the wrap
  TEST_ASSERT_FALSE(s.running());
}

void test_debounce_edges(void) {
  DebouncedInput d(30);
  d.begin(true);  // idle HIGH (pull-up)
  TEST_ASSERT_TRUE(d.update(true, 1000));
  TEST_ASSERT_TRUE(d.update(false, 1010));  // bounce starts, not stable yet
  TEST_ASSERT_FALSE(d.fell());
  TEST_ASSERT_FALSE(d.update(false, 1045));  // 35 ms low -> now reads pressed
  TEST_ASSERT_TRUE(d.fell());                // ...and the press edge fired
  TEST_ASSERT_FALSE(d.update(false, 1100));  // edge fires exactly once
  TEST_ASSERT_FALSE(d.fell());
  TEST_ASSERT_FALSE(d.fell());
  TEST_ASSERT_FALSE(d.update(true, 1110));
  TEST_ASSERT_TRUE(d.update(true, 1150));  // release edge
  TEST_ASSERT_TRUE(d.rose());
}

void run_all() {
  RUN_TEST(test_seq_boots_all_off);
  RUN_TEST(test_seq_sequential_steps);
  RUN_TEST(test_seq_hold_expiry_auto_off);
  RUN_TEST(test_seq_hold_forever_when_zero);
  RUN_TEST(test_seq_all_on_mode);
  RUN_TEST(test_btn_hold_abort_mid_cycle);
  RUN_TEST(test_btn_run_lock_ignores);
  RUN_TEST(test_btn_restart_mid_cycle);
  RUN_TEST(test_seq_manual_override);
  RUN_TEST(test_polarity_helper);
  RUN_TEST(test_seq_rollover_safe);
  RUN_TEST(test_debounce_edges);
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
