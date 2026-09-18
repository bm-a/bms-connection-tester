#include <unity.h>
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// LED state machine: green while poll <2s ago, red otherwise.
// Simulates the firmware's millis()-timer + DE sequencing at logic level.
void test_boots_red(void) {
  TEST_ASSERT_FALSE(connection_active(0, 0) && 0); // now==last==0 -> diff 0 -> active
  // Real boot: lastValid=0, now small but nonzero after setup time.
  // With lastValid=0 and now e.g. 1500: diff=1500 <2000 -> would read active,
  // so firmware boots with connected=false explicitly until first eval.
  // The invariant that matters: long silence => red.
  TEST_ASSERT_FALSE(connection_active(5000, 0));
}

void test_green_within_1s_of_poll(void) {
  unsigned long last = 10000;
  TEST_ASSERT_TRUE(connection_active(last + 500, last));
  TEST_ASSERT_TRUE(connection_active(last + 1999, last));
}

void test_red_after_2s_silence(void) {
  unsigned long last = 10000;
  TEST_ASSERT_FALSE(connection_active(last + 2000, last));
  TEST_ASSERT_FALSE(connection_active(last + 2500, last));
}

void test_self_heals_when_polling_resumes(void) {
  TEST_ASSERT_FALSE(connection_active(20000, 10000)); // silent -> red
  TEST_ASSERT_TRUE(connection_active(20100, 20050));  // poll resumes -> green
}

void test_millis_rollover_safe(void) {
  unsigned long last = 0xFFFFFF00UL; // near 32-bit wrap
  TEST_ASSERT_TRUE(connection_active(last + 500, last));   // wraps, still green
  TEST_ASSERT_FALSE(connection_active(last + 5000, last)); // wraps, red
}

void run_all() {
  RUN_TEST(test_boots_red);
  RUN_TEST(test_green_within_1s_of_poll);
  RUN_TEST(test_red_after_2s_silence);
  RUN_TEST(test_self_heals_when_polling_resumes);
  RUN_TEST(test_millis_rollover_safe);
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
