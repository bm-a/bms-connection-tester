#include <unity.h>
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// Adaptive window via PollTracker (since v1.1). Boot red, green on polls,
// red after silence, self-heal, rollover-safe, adaptive to slow polls.
void test_tracker_boots_red(void) {
  PollTracker t;
  TEST_ASSERT_FALSE(t.active(0));
  TEST_ASSERT_FALSE(t.active(1500));
}

void test_tracker_green_fast(void) {
  PollTracker t;
  t.note_poll(10000);
  TEST_ASSERT_TRUE(t.active(10500));
  TEST_ASSERT_TRUE(t.active(11999));
}

void test_tracker_red_after_window(void) {
  PollTracker t;
  t.note_poll(10000);
  TEST_ASSERT_FALSE(t.active(12000));  // boot threshold 2000
  TEST_ASSERT_FALSE(t.active(15000));
}

void test_tracker_self_heals(void) {
  PollTracker t;
  t.note_poll(10000);
  TEST_ASSERT_FALSE(t.active(20000));
  t.note_poll(20050);
  TEST_ASSERT_TRUE(t.active(20100));
}

void test_tracker_adapts_to_slow_poll(void) {
  PollTracker t;
  unsigned long now = 1000;
  for (int i = 0; i < 10; i++) { t.note_poll(now); now += 5000; }
  // EMA=5000 -> threshold clamps to 10000: green 9 s after last poll...
  TEST_ASSERT_TRUE(t.active(now - 5000 + 9000));
  // ...red past the clamp.
  TEST_ASSERT_FALSE(t.active(now - 5000 + 11000));
}

void test_tracker_threshold_bounds(void) {
  PollTracker t;
  unsigned long now = 1000;
  for (int i = 0; i < 50; i++) { t.note_poll(now); now += 50; }  // frantic
  TEST_ASSERT_EQUAL_UINT(2000, t.threshold_ms());  // floored at 2 s
  PollTracker s;
  now = 1000;
  for (int i = 0; i < 50; i++) { s.note_poll(now); now += 30000; }  // glacial
  TEST_ASSERT_EQUAL_UINT(10000, s.threshold_ms());  // capped at 10 s
}

void test_tracker_rollover_safe(void) {
  PollTracker t;
  t.note_poll(0xFFFFFF00UL);
  TEST_ASSERT_TRUE(t.active(0xFFFFFF00UL + 500));
  TEST_ASSERT_FALSE(t.active(0xFFFFFF00UL + 5000));
}

// v1.0 compat: fixed 2 s helper still behaves.
void test_legacy_connection_active(void) {
  TEST_ASSERT_FALSE(connection_active(5000, 0));
  TEST_ASSERT_TRUE(connection_active(10500, 10000));
}

void run_all() {
  RUN_TEST(test_tracker_boots_red);
  RUN_TEST(test_tracker_green_fast);
  RUN_TEST(test_tracker_red_after_window);
  RUN_TEST(test_tracker_self_heals);
  RUN_TEST(test_tracker_adapts_to_slow_poll);
  RUN_TEST(test_tracker_threshold_bounds);
  RUN_TEST(test_tracker_rollover_safe);
  RUN_TEST(test_legacy_connection_active);
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
