#include <unity.h>
#include "../../src/relay_ctrl.h"

void setUp(void) {}
void tearDown(void) {}

// v2.3 sequencer: boot OFF, sequential stepping, ALL-ON, chase wave,
// relay count, 3 button modes, ms hold expiry, manual override,
// polarity helper, rollover safety.

static Bms2Config def_cfg() {
  Bms2Config c;
  c.step_delay_ms = 500;
  c.hold_seq_ms = 30000;
  c.allon_stagger_ms = 0;  // legacy tests: true at-once unless stated
  c.relay_count = 8;
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
  c.hold_seq_ms = 2000;
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
  c.hold_seq_ms = 0;
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
  c.hold_all_ms = 5000;
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
  s.stopAll(30000);  // clears forces too
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
  c.hold_seq_ms = 2000;
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

void test_seq_count_limits_scope(void) {
  Bms2Config c = def_cfg();
  c.relay_count = 4;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(10000 + 3 * 500 + 100);  // R1..R4 on, hold entered
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
  for (uint8_t i = 4; i < RELAY_COUNT; i++) TEST_ASSERT_FALSE(s.relayOn(i));
  // ALL-ON also scoped to the first N.
  c.relay_mode = RELAY_ALL_ON;
  s.start(20000);
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
  TEST_ASSERT_FALSE(s.relayOn(4));
}

void test_seq_chase_wave(void) {
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 0;  // sweep forever until stopped
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());  // single lit relay
  s.tick(10500);  // grid advance -> BBM gap (R15): all off...
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(10500 + CHASE_BBM_MS);  // ...then R2 lights, R1 stays off
  TEST_ASSERT_FALSE(s.relayOn(0));
  TEST_ASSERT_TRUE(s.relayOn(1));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  s.tick(11000);  // next advance parks all-OFF again...
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(11000 + CHASE_BBM_MS);  // ...then R3
  TEST_ASSERT_TRUE(s.relayOn(2));
  // Full sweep wraps R8 -> R1 (one grid step per tick: resolve, then gap,
  // then light — the wave never skips, it just steps once per tick call).
  for (int k = 3; k <= 7; k++) {
    s.tick(10000 + k * 500);  // advance -> gap
    TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
    s.tick(10000 + k * 500 + CHASE_BBM_MS);  // gap over -> exactly R(k+1)
    TEST_ASSERT_TRUE(s.relayOn((uint8_t)k));
    TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  }
  s.tick(10000 + 8 * 500);  // 8th advance -> gap
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(10000 + 8 * 500 + CHASE_BBM_MS);  // wraps to R1
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  // Runs forever with hold 0; STOP ALL ends it.
  s.tick(1000000);
  TEST_ASSERT_TRUE(s.running());
  s.stopAll(1000000);
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_seq_chase_bbm_gap(void) {
  // R15/R19: release is slower than pull-in, so every chase advance parks
  // all-OFF for CHASE_BBM_MS. Bitmask weight: 1 while lit, 0 in the gap.
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 0;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  s.tick(10499);
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());  // still R1
  s.tick(10500);  // grid advance -> BBM gap, nothing lit
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(10519);
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(10520);  // gap over -> exactly R2
  TEST_ASSERT_TRUE(s.relayOn(1));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
}

void test_seq_chase_count_wrap_and_hold(void) {
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_CHASE;
  c.relay_count = 3;
  c.chase_sweeps = 2;  // auto-hold: 2 sweeps x 3 relays x 500 ms = 3000 ms
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);  // hold expires at 13000
  s.tick(10500);  // first advance -> BBM gap
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(10500 + CHASE_BBM_MS);
  TEST_ASSERT_TRUE(s.relayOn(1));
  s.tick(11000);  // gap again...
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(11000 + CHASE_BBM_MS);
  TEST_ASSERT_TRUE(s.relayOn(2));
  s.tick(11500);  // gap...
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(11500 + CHASE_BBM_MS);  // ...wraps within first 3
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  TEST_ASSERT_FALSE(s.relayOn(3));  // beyond count never lights
  s.tick(12999);
  TEST_ASSERT_TRUE(s.running());
  s.tick(13000);  // auto-hold expired -> auto OFF
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_spoof_pin_sanitize(void) {
  // Allowlist: proven-safe free DIOs pass through untouched.
  const uint8_t ok[] = {1, 2, 21, 38, 39, 40, 41, 42, 43, 44, 47};
  for (uint8_t i = 0; i < sizeof(ok); i++)
    TEST_ASSERT_EQUAL_UINT8(ok[i], sanitize_spoof_pin(ok[i]));
  // Everything else (UART, relays, button, LEDs, USB, strapping, flash,
  // kill switch) falls back to 21.
  const uint8_t bad[] = {0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                         16, 17, 18, 19, 20, 26, 33, 37, 45, 46, 48, 99};
  for (uint8_t i = 0; i < sizeof(bad); i++)
    TEST_ASSERT_EQUAL_UINT8(21, sanitize_spoof_pin(bad[i]));
  // Chase auto-hold retunes with count+step (sweeps x relays x step ms).
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 2;  // 2 x 8 x 500 ms
  RelaySequencer s;
  s.begin(&c);
  TEST_ASSERT_EQUAL_UINT32(8000, s.holdForMode());
  c.chase_sweeps = 0;
  TEST_ASSERT_EQUAL_UINT32(0, s.holdForMode());  // 0 = sweep forever
}

void test_seq_hold_ms_precise(void) {
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 1500;  // millisecond granularity (no more whole seconds)
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);  // all on, hold until 15000
  s.tick(14999);
  TEST_ASSERT_TRUE(s.running());
  s.tick(15000);
  TEST_ASSERT_FALSE(s.running());
}

void test_seq_hold_per_mode(void) {
  // Each mode obeys its own hold: short sequential settle, long ALL-ON soak.
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 1000;
  c.hold_all_ms = 60000;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);  // sequential steps done at 13500, hold 1000 -> off 14500
  s.tick(13500);
  s.tick(14499);
  TEST_ASSERT_TRUE(s.running());
  s.tick(14500);
  TEST_ASSERT_FALSE(s.running());
  c.relay_mode = RELAY_ALL_ON;
  s.start(20000);  // ALL-ON: off at 20000 + 60000, NOT at the seq hold
  s.tick(20000 + 1500);
  TEST_ASSERT_TRUE(s.running());
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());
  s.tick(20000 + 60000);
  TEST_ASSERT_FALSE(s.running());
  // holdForMode reports the active mode's hold.
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 2;  // 2 x 8 x 500 ms = 8000 ms auto-hold
  TEST_ASSERT_EQUAL_UINT32(8000, s.holdForMode());
  c.chase_sweeps = 0;
  TEST_ASSERT_EQUAL_UINT32(0, s.holdForMode());
  c.relay_mode = RELAY_SEQUENTIAL;
  TEST_ASSERT_EQUAL_UINT32(1000, s.holdForMode());
}

void test_seq_effcount_clamp(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  c.relay_count = 0;
  s.begin(&c);
  TEST_ASSERT_EQUAL_UINT8(1, s.effCount());
  c.relay_count = 99;
  TEST_ASSERT_EQUAL_UINT8(8, s.effCount());
  c.relay_count = 6;
  TEST_ASSERT_EQUAL_UINT8(6, s.effCount());
}

void test_loop_pause_restart(void) {
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 1000;
  c.loop_enabled = true;
  c.cycle_pause_ms = 2000;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);  // steps done, hold 13500 -> 14500
  s.tick(14499);
  TEST_ASSERT_TRUE(s.running());
  s.tick(14500);  // cycle 1 done -> pause (all off, still "running")
  TEST_ASSERT_EQUAL_UINT(1, s.cyclesDone());
  TEST_ASSERT_TRUE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(16499);
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());  // still resting (coil cooling)
  s.tick(16500);  // pause elapsed -> cycle 2 starts with R1
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  s.tick(20000);  // steps done again, hold 20000 -> 21000
  s.tick(21000);  // cycle 2 done
  TEST_ASSERT_EQUAL_UINT(2, s.cyclesDone());
  TEST_ASSERT_TRUE(s.running());  // limit 0 = loops forever
}

void test_loop_limit_stops(void) {
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 1000;
  c.loop_enabled = true;
  c.cycle_pause_ms = 2000;
  c.cycle_limit = 2;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);
  s.tick(14500);  // cycle 1 -> pause
  TEST_ASSERT_TRUE(s.running());
  s.tick(16500);  // cycle 2 starts
  s.tick(20000);
  s.tick(21000);  // cycle 2 = limit -> STOP, not pause
  TEST_ASSERT_EQUAL_UINT(2, s.cyclesDone());
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
}

void test_no_loop_single_shot(void) {
  Bms2Config c = def_cfg();  // loop_enabled=false default: v2.0 behavior
  c.hold_seq_ms = 1000;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);
  s.tick(14500);
  TEST_ASSERT_EQUAL_UINT(1, s.cyclesDone());
  TEST_ASSERT_FALSE(s.running());  // stopped, no pause-restart
}

void test_reverse_direction(void) {
  Bms2Config c = def_cfg();
  c.seq_dir = 1;
  c.relay_count = 4;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);  // reverse: last relay first
  TEST_ASSERT_TRUE(s.relayOn(3));
  TEST_ASSERT_FALSE(s.relayOn(0));
  s.tick(10500);
  TEST_ASSERT_TRUE(s.relayOn(2));
  TEST_ASSERT_EQUAL_UINT8(2, s.onCount());
  // Chase reverses too.
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 0;
  s.start(20000);
  TEST_ASSERT_TRUE(s.relayOn(3));
  s.tick(20500);  // advance -> BBM gap
  TEST_ASSERT_EQUAL_UINT8(0, s.onCount());
  s.tick(20500 + CHASE_BBM_MS);
  TEST_ASSERT_TRUE(s.relayOn(2));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
}

void test_allon_stagger_ramps(void) {
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_ALL_ON;
  c.relay_count = 4;
  c.allon_stagger_ms = 400;  // inrush ramp, not a true at-once slam
  c.hold_all_ms = 5000;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());  // only R1 immediately
  s.tick(10400);
  TEST_ASSERT_EQUAL_UINT8(2, s.onCount());
  s.tick(11200);  // all 4 on -> hold_all window (11200 -> 16200)
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
  s.tick(16199);
  TEST_ASSERT_TRUE(s.running());
  s.tick(16200);
  TEST_ASSERT_FALSE(s.running());
  // Stagger 0 = true at-once (v2.0 behavior).
  c.allon_stagger_ms = 0;
  s.start(20000);
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
}

void test_counters(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  TEST_ASSERT_EQUAL_UINT(0, s.cyclesDone());
  TEST_ASSERT_EQUAL_UINT(0, s.actuations());
  s.start(10000);
  s.tick(13500);  // 8 sequential actuations
  TEST_ASSERT_EQUAL_UINT(8, s.actuations());
  s.tick(13500 + 30000);  // hold_seq default 30000 -> cycle done
  TEST_ASSERT_EQUAL_UINT(1, s.cyclesDone());
  s.stopAll(50000);  // stop preserves QC counters; begin resets them
  TEST_ASSERT_EQUAL_UINT(1, s.cyclesDone());
  TEST_ASSERT_EQUAL_UINT(8, s.actuations());
  s.begin(&c);
  TEST_ASSERT_EQUAL_UINT(0, s.cyclesDone());
  TEST_ASSERT_EQUAL_UINT(0, s.actuations());
}

// R30: mandatory all-OFF dead-band between stop and next start.
void test_start_deadband_after_stop(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  TEST_ASSERT_TRUE(s.start(10000));
  TEST_ASSERT_TRUE(s.running());
  s.stopAll(20000);
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_FALSE(s.start(20000));  // immediate restart refused
  TEST_ASSERT_FALSE(s.start(20499));
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_TRUE(s.start(20500));  // dead-band elapsed
  TEST_ASSERT_TRUE(s.running());
  TEST_ASSERT_TRUE(s.relayOn(0));
}

// R29: forcing a tile mid-chase idles the wave first — never two ON at once.
void test_force_in_chase_idles_wave(void) {
  Bms2Config c = def_cfg();
  c.relay_mode = RELAY_CHASE;
  c.chase_sweeps = 0;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(10500 + CHASE_BBM_MS);
  TEST_ASSERT_TRUE(s.running());
  s.setForced(5, true);  // wave drops to IDLE, tile becomes the only ON
  TEST_ASSERT_FALSE(s.running());
  TEST_ASSERT_TRUE(s.relayOn(5));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  TEST_ASSERT_EQUAL_UINT(0, s.cyclesDone());  // no phantom cycle counted
}

// R22: count shrink mid-run drops out-of-range outputs the same tick.
void test_count_shrink_live_drop(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);
  TEST_ASSERT_EQUAL_UINT8(8, s.onCount());
  c.relay_count = 4;  // edited live, no countChanged() call
  s.tick(13600);
  for (uint8_t i = 4; i < RELAY_COUNT; i++) TEST_ASSERT_FALSE(s.relayOn(i));
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
}

// R8: countChanged() clears out-of-range forces + clamps the step pointer.
void test_count_changed_clears_forces(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  s.setForced(6, true);
  s.setForced(2, true);
  s.start(10000);
  c.relay_count = 4;
  s.countChanged();
  TEST_ASSERT_FALSE(s.relayOn(6));  // stale force gone
  TEST_ASSERT_TRUE(s.relayOn(2));   // in-range force kept
}

// R23: regrow after a shrink must not resurrect a stale force.
void test_count_regrow_no_resurrect(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  s.setForced(6, true);
  c.relay_count = 4;
  s.countChanged();
  TEST_ASSERT_FALSE(s.relayOn(6));
  c.relay_count = 8;
  s.countChanged();
  s.tick(50000);
  TEST_ASSERT_FALSE(s.relayOn(6));  // stays off: no new operator action
}

// R24 (sequencer half): hold 0 never expires even with loop on.
// (The web layer rejects hold==0 && loop at save time; the sequencer just
// never produces a cycle from an unterminated hold.)
void test_hold0_loop_never_cycles(void) {
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 0;
  c.loop_enabled = true;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);
  s.tick(10000000);
  TEST_ASSERT_TRUE(s.running());
  TEST_ASSERT_EQUAL_UINT(0, s.cyclesDone());
}

// R31: RESTART preserves manual forces (only STOP ALL clears them).
void test_restart_preserves_forces(void) {
  Bms2Config c = def_cfg();
  c.button_mode = BTN_RESTART;
  RelaySequencer s;
  s.begin(&c);
  s.setForced(7, true);
  handle_button_press(s, c, 10000);
  s.tick(12000);
  handle_button_press(s, c, 12000);  // restart from R1, tile kept
  TEST_ASSERT_TRUE(s.relayOn(7));
  TEST_ASSERT_TRUE(s.relayOn(0));
  TEST_ASSERT_EQUAL_UINT8(2, s.onCount());
}

// R11: mid-cycle timing edits apply next cycle, never mid-flight.
void test_midcycle_edit_latched(void) {
  Bms2Config c = def_cfg();
  c.hold_seq_ms = 1000;
  c.loop_enabled = true;
  c.cycle_pause_ms = 2000;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  s.tick(13500);  // steps done on the 500 ms grid, hold 13500 -> 14500
  c.step_delay_ms = 50;  // edit mid-cycle: must not disturb this cycle
  c.hold_seq_ms = 9000;
  s.tick(14500);  // cycle 1 ends on the ORIGINAL hold
  TEST_ASSERT_EQUAL_UINT(1, s.cyclesDone());
  s.tick(16500);  // pause elapsed -> cycle 2 starts...
  // ...on the NEW 50 ms grid: R2 asserts at 16550, not 17000.
  s.tick(16549);
  TEST_ASSERT_FALSE(s.relayOn(1));
  s.tick(16550);
  TEST_ASSERT_TRUE(s.relayOn(1));
}

// Start while running restarts immediately (no stop => no dead-band).
void test_start_while_running_restarts(void) {
  Bms2Config c = def_cfg();
  RelaySequencer s;
  s.begin(&c);
  TEST_ASSERT_TRUE(s.start(10000));
  s.tick(13000);
  TEST_ASSERT_EQUAL_UINT8(7, s.onCount());
  TEST_ASSERT_TRUE(s.start(14000));
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  TEST_ASSERT_TRUE(s.relayOn(0));
}

// R5: fresh boxes stagger ALL-ON by default (never slam 8 contactors).
void test_allon_default_stagger_ramps(void) {
  Bms2Config c;  // struct defaults: stagger 50 ms
  TEST_ASSERT_EQUAL_UINT16(50, c.allon_stagger_ms);
  TEST_ASSERT_EQUAL_UINT16(250, c.step_delay_ms);
  TEST_ASSERT_EQUAL_UINT32(2000, c.cycle_pause_ms);
  c.relay_mode = RELAY_ALL_ON;
  c.relay_count = 4;
  RelaySequencer s;
  s.begin(&c);
  s.start(10000);
  TEST_ASSERT_EQUAL_UINT8(1, s.onCount());
  s.tick(10050);
  TEST_ASSERT_EQUAL_UINT8(2, s.onCount());
  s.tick(10150);
  TEST_ASSERT_EQUAL_UINT8(4, s.onCount());
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
  RUN_TEST(test_seq_count_limits_scope);
  RUN_TEST(test_seq_chase_wave);
  RUN_TEST(test_seq_chase_count_wrap_and_hold);
  RUN_TEST(test_spoof_pin_sanitize);
  RUN_TEST(test_seq_hold_ms_precise);
  RUN_TEST(test_seq_hold_per_mode);
  RUN_TEST(test_seq_effcount_clamp);
  RUN_TEST(test_loop_pause_restart);
  RUN_TEST(test_loop_limit_stops);
  RUN_TEST(test_no_loop_single_shot);
  RUN_TEST(test_reverse_direction);
  RUN_TEST(test_allon_stagger_ramps);
  RUN_TEST(test_counters);
  RUN_TEST(test_seq_chase_bbm_gap);
  RUN_TEST(test_start_deadband_after_stop);
  RUN_TEST(test_force_in_chase_idles_wave);
  RUN_TEST(test_count_shrink_live_drop);
  RUN_TEST(test_count_changed_clears_forces);
  RUN_TEST(test_count_regrow_no_resurrect);
  RUN_TEST(test_hold0_loop_never_cycles);
  RUN_TEST(test_restart_preserves_forces);
  RUN_TEST(test_midcycle_edit_latched);
  RUN_TEST(test_start_while_running_restarts);
  RUN_TEST(test_allon_default_stagger_ramps);
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
