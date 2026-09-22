#include <unity.h>
#include "../../src/relay_ctrl.h"

void setUp(void) {}
void tearDown(void) {}

// v2.6 daily meter-test counting (R32-R40): software-only APPROXIMATE
// counter, no button. Link gaps >= 3 s = reseat = new meter; steady GREEN
// across RESTARTs/retries = same meter; flickers stay; mid-cycle gaps
// defer their close until IDLE.

static Bms2Config fast_cfg() {
  Bms2Config c;
  c.step_delay_ms = 100;
  c.hold_seq_ms = 500;
  c.allon_stagger_ms = 0;
  c.relay_count = 2;  // short cycles: R1 @t, R2 @t+100, hold till +600
  c.relay_mode = RELAY_SEQUENTIAL;
  c.button_mode = BTN_RESTART;
  c.active_low = true;
  return c;
}

// Run one full sequential cycle to completion (hold expiry -> stopAll).
// t is advanced past the post-stop dead-band so the caller can restart.
static void run_full_cycle(RelaySequencer &s, unsigned long &t) {
  TEST_ASSERT_TRUE(s.start(t));
  s.tick(t + 100);   // R2 on -> HOLD (hold_until = t+600)
  TEST_ASSERT_TRUE(s.running());
  s.tick(t + 600);   // hold expired -> cycleDone -> stopAll (no loop)
  TEST_ASSERT_FALSE(s.running());
  t += 1200;  // clear the 500 ms R12 dead-band for the next start
}

// Feed a RED gap of gap_ms then GREEN (like the main loop would see it).
static void reseat(RelaySequencer &s, unsigned long &t, unsigned long gap_ms) {
  s.meter().noteLink(false, t, s.running());  // falling edge
  t += gap_ms;
  s.meter().noteLink(true, t, s.running());  // rising edge
  s.meter().pollIdle(s.running());
}

// T1: first GREEN sighting since boot opens meter #1, no verdict.
void test_meter_first_sighting_opens_meter(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 5000;
  s.meter().noteLink(true, t, s.running());
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

// T2: RESTART retries with the meter plugged in never open a new meter.
void test_meter_restart_same_link_no_new_meter(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 10000;
  s.meter().noteLink(true, t, s.running());  // meter #1 plugged in
  TEST_ASSERT_TRUE(s.start(t));              // attempt 1
  handle_button_press(s, c, t + 50);         // RESTART while running
  handle_button_press(s, c, t + 100);        // RESTART again
  s.tick(t + 150);  // link steady GREEN the whole time
  s.meter().noteLink(true, t + 150, s.running());
  TEST_ASSERT_EQUAL_UINT32(3, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

// T3: full cycle then a 5 s reseat gap = one pass verdict, meter #2.
void test_meter_cycle_then_gap_is_pass(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 20000;
  s.meter().noteLink(true, t, s.running());
  run_full_cycle(s, t);
  reseat(s, t, 5000);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

// T4: retry loop (abort + complete) then gap collapses to ONE pass verdict.
void test_meter_retry_loop_single_pass_verdict(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 30000;
  s.meter().noteLink(true, t, s.running());
  TEST_ASSERT_TRUE(s.start(t));  // attempt 1: aborted mid-cycle (fault retry)
  s.tick(t + 50);
  s.stopAll(t + 50);
  t += 1200;
  run_full_cycle(s, t);  // attempt 2: completes
  reseat(s, t, 5000);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

// T5: brief flickers (< 3 s RED) stay on the same meter — no phantom units.
void test_meter_short_flicker_same_meter(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 40000;
  s.meter().noteLink(true, t, s.running());
  reseat(s, t, 500);   // slow poll / noise blip
  reseat(s, t, 2999);  // just under the threshold
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().attempts);
  reseat(s, t, 3000);  // exactly the threshold: new meter
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
}

// T6: abort with NO completion then gap = fail verdict (fail means
// "closed out with no completed cycle" — the only honest fail signal).
void test_meter_abort_then_gap_is_fail(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 50000;
  s.meter().noteLink(true, t, s.running());
  TEST_ASSERT_TRUE(s.start(t));
  s.tick(t + 50);
  s.stopAll(t + 50);  // operator aborts the faulty run, swaps the unit
  t += 1200;
  reseat(s, t, 5000);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().fail);
}

// T7: day reset zeroes the batch; boot restore reloads persisted totals
// (setTotals = what web_ui overlays from NVS); boot never zeroes by itself.
void test_meter_day_reset_and_boot_restore(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 60000;
  s.meter().noteLink(true, t, s.running());
  run_full_cycle(s, t);
  reseat(s, t, 5000);
  s.meter().dayReset();
  // Link is GREEN (unit seated) so it opens as today's meter #1 at once.
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
  // Simulated reboot: begin() zeroes, then NVS totals overlay.
  s.begin(&c);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().meters);
  s.meter().setTotals(7, 9, 6, 1);
  TEST_ASSERT_EQUAL_UINT32(7, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(9, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(6, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().fail);
}

// T8: simulated day boundary — 200 meters, snapshot, reset, restart at zero.
void test_meter_day_boundary(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 70000;
  s.meter().noteLink(true, t, s.running());
  for (int i = 0; i < 200; i++) {
    run_full_cycle(s, t);
    reseat(s, t, 5000);
  }
  TEST_ASSERT_EQUAL_UINT32(201, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(200, s.meter().attempts);
  TEST_ASSERT_EQUAL_UINT32(200, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
  uint32_t snap_m = s.meter().meters, snap_a = s.meter().attempts;
  uint32_t snap_p = s.meter().pass, snap_f = s.meter().fail;
  TEST_ASSERT_EQUAL_UINT32(201, snap_m);
  TEST_ASSERT_EQUAL_UINT32(200, snap_a);
  TEST_ASSERT_EQUAL_UINT32(200, snap_p);
  TEST_ASSERT_EQUAL_UINT32(0, snap_f);
  s.meter().dayReset();  // operator declares the new day (no RTC)
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);  // seated unit = today's #1
  run_full_cycle(s, t);
  reseat(s, t, 5000);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().attempts);
}

// T9: no-RTC soak — gaps across the millis() wrap, refused starts open
// nothing, 50k attempts without corruption.
void test_meter_millis_wrap_and_soak(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 0xFFFFFF00UL;  // 256 ms before the 32-bit wrap
  s.meter().noteLink(true, t, s.running());
  run_full_cycle(s, t);            // completes across the wrap
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().attempts);
  reseat(s, t, 5000);              // gap across the wrap closes meter #1
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  // Refused start (inside the R12 dead-band) opens no attempt.
  unsigned long u = t;  // t already advanced past dead-band by helper
  TEST_ASSERT_TRUE(s.start(u));
  s.stopAll(u + 10);
  TEST_ASSERT_FALSE(s.start(u + 100));  // 90 ms later: refused
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().attempts);
  unsigned long v = u + 1200;
  for (int i = 0; i < 50000; i++) {
    if (s.start(v)) {
      s.tick(v + 100);
      s.tick(v + 600);
    }
    v += 1200;
  }
  TEST_ASSERT_EQUAL_UINT32(50002, s.meter().attempts);
  reseat(s, v, 5000);  // 50k cycles -> one pass verdict for the open meter
  TEST_ASSERT_EQUAL_UINT32(3, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().pass);
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

// T10: reseat mid-cycle defers the close until IDLE — the in-flight cycle's
// pass verdict is attributed to the correct (previous) meter.
void test_meter_midcycle_gap_defers_close(void) {
  Bms2Config c = fast_cfg();
  RelaySequencer s;
  s.begin(&c);
  unsigned long t = 80000;
  s.meter().noteLink(true, t, s.running());  // meter #1
  TEST_ASSERT_TRUE(s.start(t));              // attempt opens
  s.tick(t + 100);                           // HOLD
  // Operator yanks the unit mid-cycle (gap elapses while RUNNING).
  s.meter().noteLink(false, t + 100, s.running());
  t += 5100;
  s.meter().noteLink(true, t, s.running());  // GREEN back, still running
  s.meter().pollIdle(s.running());
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().meters);  // deferred: not closed yet
  s.tick(t);  // hold (until 80600) long expired -> cycleDone -> stopAll
  TEST_ASSERT_FALSE(s.running());
  s.meter().pollIdle(s.running());  // IDLE eval lands the pending close
  TEST_ASSERT_EQUAL_UINT32(2, s.meter().meters);
  TEST_ASSERT_EQUAL_UINT32(1, s.meter().pass);  // in-flight cycle counted
  TEST_ASSERT_EQUAL_UINT32(0, s.meter().fail);
}

static void run_all() {
  RUN_TEST(test_meter_first_sighting_opens_meter);
  RUN_TEST(test_meter_restart_same_link_no_new_meter);
  RUN_TEST(test_meter_cycle_then_gap_is_pass);
  RUN_TEST(test_meter_retry_loop_single_pass_verdict);
  RUN_TEST(test_meter_short_flicker_same_meter);
  RUN_TEST(test_meter_abort_then_gap_is_fail);
  RUN_TEST(test_meter_day_reset_and_boot_restore);
  RUN_TEST(test_meter_day_boundary);
  RUN_TEST(test_meter_millis_wrap_and_soak);
  RUN_TEST(test_meter_midcycle_gap_defers_close);
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
