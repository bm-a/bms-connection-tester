#pragma once
#include <stdint.h>
#include <stddef.h>
#include "bms_protocol.h"

// v2.3 relay sequencer + 2-stage spoof + debounce + config.
// Hardware-independent (no Arduino dependency) so it compiles on the host
// for Unity tests, exactly like bms_protocol.*.
// The v1.x base (parser, dispatcher, tracker, golden frames, LEDs, STATUS?)
// is FROZEN and untouched — this file only adds selection + GPIO logic.

// Logical relay indexing: 0..7 maps to R1..R8 (physical pins in main.cpp).
// v2.3: only the first cfg.relay_count relays participate; the rest stay OFF.
#define RELAY_COUNT 8

// Relay run mode (web "Mode").
enum RelayMode : uint8_t {
  RELAY_SEQUENTIAL = 0,  // R1 -> R(n) with step delay, cumulative
  RELAY_ALL_ON = 1,      // first n at once
  RELAY_CHASE = 2,       // v2.3: single lit relay sweeping R1 -> R(n), wraps
};

// Button-press behavior (web "Button").
enum ButtonMode : uint8_t {
  BTN_HOLD_ABORT = 0,  // hold X s then auto-OFF; re-press mid-cycle = OFF now
  BTN_RUN_LOCK = 1,    // run to completion; presses ignored until finished
  BTN_RESTART = 2,     // re-press restarts the cycle from R1, any stage
};

// Persistent config (stored in NVS by web_ui; plain struct here for tests).
struct Bms2Config {
  uint16_t step_delay_ms = 500;   // sequential gap R(n) -> R(n+1)
  // v2.3 per-mode holds (ms each; 0 = stay ON forever until stopped).
  // Sequential wants a short settle; ALL-ON burn-in wants a long soak.
  uint32_t hold_seq_ms = 30000;
  // v2.3.1 chase auto-hold: full sweeps (0 = sweep forever until stopped).
  // Effective hold = sweeps x relay-count x step, recomputed at start(), so
  // it retunes itself when count/step change. Replaces hold_chase_ms.
  uint8_t chase_sweeps = 3;
  uint32_t hold_all_ms = 300000;
  uint8_t relay_count = 8;        // v2.3: first N relays participate (1..8)
  uint8_t relay_mode = RELAY_SEQUENTIAL;
  uint8_t button_mode = BTN_HOLD_ABORT;
  // v2.3 industrial pack (all additive; defaults = v2.0 behavior).
  bool loop_enabled = false;      // repeat the cycle until stopped/limit
  uint32_t cycle_pause_ms = 5000;  // rest between looped cycles (coil cooling)
  uint16_t cycle_limit = 0;       // 0 = forever, else stop after N cycles
  uint16_t allon_stagger_ms = 0;  // 0 = true at-once; >0 = ramp gap (inrush)
  uint8_t seq_dir = 0;            // 0 = R1->Rn, 1 = Rn->R1
  bool boot_autostart = false;    // start the configured mode on boot
  char relay_label[RELAY_COUNT][12];  // dashboard tile names (NVS, QC labels)
  Bms2Config() {
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
      relay_label[i][0] = 'R';
      relay_label[i][1] = (char)('1' + i);
      relay_label[i][2] = '\0';
    }
  }
  bool active_low = true;         // SmartElex class: LOW = relay ON
  bool button_invert = false;     // false: press pulls pin LOW (pull-up)
  bool spoof_invert = false;      // false: trigger pulls pin LOW
  bool spoof_enabled = true;
  // v2.3.1 configurable trigger GPIO (was hardwired 21). Only proven-safe
  // free DIOs are accepted; anything else falls back to 21 (see below).
  uint8_t spoof_pin = 21;
  // v2.3 two-stage spoof: stage 1 fires first, then stage 2, then revert.
  // Stage 1 defaults = "100" (realistic full pack); stage 2 = 88.8/188
  // over-range pattern. Upgraders: legacy single-stage values migrate to
  // stage 2 (see web_ui NVS v2->v3 migration).
  uint16_t spoof_v_tenth = 1000;  // stage 1: 100.0 V display units (x0.1)
  uint16_t spoof_a_tenth = 1000;  // stage 1: 100.0 A display units (x0.1)
  uint16_t spoof_c_tenth = 1000;  // stage 1: 100.0 C display units (x0.1)
  uint8_t spoof_soc = 100;        // stage 1: 100 %
  uint16_t spoof_seconds = 5;     // stage 1 duration, 1..120
  uint16_t s2_v_tenth = 888;      // stage 2: 88.8 V display units (x0.1)
  uint16_t s2_a_tenth = 888;      // stage 2: 88.8 A display units (x0.1)
  uint16_t s2_c_tenth = 888;      // stage 2: 88.8 C display units (x0.1)
  uint8_t s2_soc = 188;           // stage 2: deliberately out-of-range pattern
  uint16_t s2_seconds = 10;       // stage 2 duration, 1..120
};

// Physical pin level for a logical relay state under the polarity setting.
// Tested on host so bench behavior is proven before flashing.
inline uint8_t relay_pin_level(bool logical_on, bool active_low) {
  // Arduino HIGH=1 / LOW=0 assumed by caller mapping.
  if (active_low) return logical_on ? 0 : 1;
  return logical_on ? 1 : 0;
}

// v2.3.1 spoof-trigger GPIO allowlist: proven-safe free DIOs only.
// Everything else (UART, relays, button, LEDs/RGB, USB, strapping, flash,
// the WiFi kill pin 18) falls back to 21. Tested on host.
inline uint8_t sanitize_spoof_pin(uint8_t p) {
  switch (p) {
    case 1: case 2: case 21: case 38: case 39: case 40: case 41: case 42:
    case 43: case 44: case 47:
      return p;
    default:
      return 21;
  }
}

// ---- 8-relay sequencer: pure millis() state machine, no delay() ----
class RelaySequencer {
 public:
  void begin(const Bms2Config *cfg);
  void start(unsigned long now);     // button press / web START
  void stopAll();                    // abort now: everything OFF, forces cleared
  void setForced(uint8_t i, bool on);// web per-relay manual override
  void clearForced();
  void tick(unsigned long now);      // advance stepping + hold expiry
  bool relayOn(uint8_t i) const;     // logical state (sequence OR forced)
  bool running() const { return phase_ != PH_IDLE; }
  uint8_t onCount() const;
  // Active relay count (first N participate, clamped 1..8).
  uint8_t effCount() const;
  // Hold for the configured mode (ms; 0 = forever).
  uint32_t holdForMode() const;
  // v2.3 QC counters (since begin(); stopAll preserves them).
  unsigned long cyclesDone() const { return cycles_done_; }
  unsigned long actuations() const { return acts_; }
 private:
  enum Phase : uint8_t { PH_IDLE, PH_RUNNING, PH_HOLD, PH_CHASE, PH_PAUSE };
  const Bms2Config *cfg_ = nullptr;
  Phase phase_ = PH_IDLE;
  uint8_t step_ = 0;                 // next relay index to switch (sequential)
  unsigned long step_at_ = 0;        // when step_ may switch
  unsigned long hold_until_ = 0;     // 0 = forever
  unsigned long pause_until_ = 0;    // v2.3 loop rest window
  unsigned long cycles_done_ = 0;    // v2.3 completed cycles (hold expiries)
  unsigned long acts_ = 0;           // v2.3 relay turn-ON edges (QC counter)
  bool seq_[RELAY_COUNT] = {false};
  bool forced_[RELAY_COUNT] = {false};
  bool forced_state_[RELAY_COUNT] = {false};
  void enterHold(unsigned long now);
  void enterPause(unsigned long now);  // v2.3 loop rest, then restart
  void cycleDone(unsigned long now);   // hold expired: loop or stop
  uint16_t stepGap() const;            // step_delay or ALL-ON stagger
  uint8_t mapStep(uint8_t k) const;    // v2.3 direction: position -> relay
  void lightStep(uint8_t k);           // light exactly position k (+counter)
};

// ---- button-press dispatcher: maps presses to sequencer ops ----
inline void handle_button_press(RelaySequencer &seq, const Bms2Config &cfg,
                                unsigned long now) {
  switch ((ButtonMode)cfg.button_mode) {
    case BTN_RUN_LOCK:
      if (!seq.running()) seq.start(now);
      break;
    case BTN_RESTART:
      seq.start(now);  // start() always restarts from R1
      break;
    case BTN_HOLD_ABORT:
    default:
      if (seq.running()) seq.stopAll();
      else seq.start(now);
      break;
  }
}

// ---- debounced digital input with edge events ----
class DebouncedInput {
 public:
  explicit DebouncedInput(unsigned long debounce_ms = 30)
      : debounce_ms_(debounce_ms) {}
  void begin(bool initial_raw);
  // Returns stable level. fell()/rose() report edges since last update().
  bool update(bool raw, unsigned long now);
  bool fell() const { return fell_; }
  bool rose() const { return rose_; }
  bool stable() const { return stable_; }
 private:
  unsigned long debounce_ms_;
  bool stable_ = true;
  bool last_raw_ = true;
  unsigned long change_at_ = 0;
  bool fell_ = false;
  bool rose_ = false;
};

// ---- spoof window: 0x03 special-values timer (v2.0 single-stage) ----
// FROZEN (kept for its unit tests). Production uses SpoofPlan below.
class SpoofWindow {
 public:
  void trigger(unsigned long now, unsigned long duration_ms);
  void cancel() { until_ = 0; armed_ = false; }
  bool active(unsigned long now) const;
 private:
  unsigned long until_ = 0;
  bool armed_ = false;
};

// ---- v2.3 two-stage spoof plan: stage 1, then stage 2, then idle ----
// stage(now): 0 = idle/reverted, 1 = stage 1 active, 2 = stage 2 active.
// Durations are milliseconds (callers convert the cfg seconds). All
// comparisons are rollover-safe.
class SpoofPlan {
 public:
  void trigger(unsigned long now, unsigned long stage1_ms,
               unsigned long stage2_ms);
  void cancel() { armed_ = false; }
  uint8_t stage(unsigned long now) const;
  bool active(unsigned long now) const { return stage(now) != 0; }
 private:
  unsigned long t0_ = 0;
  unsigned long s1_ = 0;
  unsigned long s2_ = 0;
  bool armed_ = false;
};

// ---- spoof frame builder: frozen golden + patched fields + valid checksum ----
// out must hold 34 bytes. Units: V/A raw = tenth*10 (0.01 scale),
// temp raw = 2731 + tenth (JBD 0.1K offset), SOC direct byte.
// stage: 1 = stage-1 fields, anything else (incl. 2) = stage-2 fields.
void build_spoof_frame(const Bms2Config &cfg, uint8_t stage, uint8_t out[34]);
#define SPOOF_FRAME_LEN 34

// ---- reply selection: the exact rule main.cpp's loop uses (host-tested) ----
// Returns golden/cell/name via reply_for(), except reg 0x03 reads during an
// active spoof stage answer spoof_frame. Writes/unknown stay silent.
const uint8_t *select_reply(const JbdFrame &f, const Bms2Config &cfg,
                            uint8_t spoof_stage, const uint8_t *spoof_frame,
                            size_t &out_len);
