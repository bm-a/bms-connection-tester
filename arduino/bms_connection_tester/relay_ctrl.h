#pragma once
#include <stdint.h>
#include <stddef.h>

// v2.0 relay sequencer + spoof window + debounce + config.
// Hardware-independent (no Arduino dependency) so it compiles on the host
// for Unity tests, exactly like bms_protocol.*.
// The v1.x base (parser, dispatcher, tracker, golden frames, LEDs, STATUS?)
// is FROZEN and untouched — this file only adds selection + GPIO logic.

// Logical relay indexing: 0..7 maps to R1..R8 (physical pins in main.cpp).
#define RELAY_COUNT 8

// Relay run mode (web "Mode").
enum RelayMode : uint8_t {
  RELAY_SEQUENTIAL = 0,  // R1 -> R8 with step delay
  RELAY_ALL_ON = 1,      // all 8 at once
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
  uint16_t hold_seconds = 30;     // 0 = stay ON forever until stopped
  uint8_t relay_mode = RELAY_SEQUENTIAL;
  uint8_t button_mode = BTN_HOLD_ABORT;
  bool active_low = true;         // SmartElex class: LOW = relay ON
  bool button_invert = false;     // false: press pulls pin LOW (pull-up)
  bool spoof_invert = false;      // false: trigger pulls pin LOW
  bool spoof_enabled = true;
  uint16_t spoof_v_tenth = 888;   // 88.8 V display units (x0.1)
  uint16_t spoof_a_tenth = 888;   // 88.8 A display units (x0.1)
  uint16_t spoof_c_tenth = 888;   // 88.8 C display units (x0.1)
  uint8_t spoof_soc = 188;        // deliberately out-of-range test pattern
  uint16_t spoof_seconds = 10;    // 1..120
};

// Physical pin level for a logical relay state under the polarity setting.
// Tested on host so bench behavior is proven before flashing.
inline uint8_t relay_pin_level(bool logical_on, bool active_low) {
  // Arduino HIGH=1 / LOW=0 assumed by caller mapping.
  if (active_low) return logical_on ? 0 : 1;
  return logical_on ? 1 : 0;
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
 private:
  enum Phase : uint8_t { PH_IDLE, PH_RUNNING, PH_HOLD };
  const Bms2Config *cfg_ = nullptr;
  Phase phase_ = PH_IDLE;
  uint8_t step_ = 0;                 // next relay index to switch (sequential)
  unsigned long step_at_ = 0;        // when step_ may switch
  unsigned long hold_until_ = 0;     // 0 = forever
  bool seq_[RELAY_COUNT] = {false};
  bool forced_[RELAY_COUNT] = {false};
  bool forced_state_[RELAY_COUNT] = {false};
  void enterHold(unsigned long now);
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

// ---- spoof window: 0x03 special-values timer ----
class SpoofWindow {
 public:
  void trigger(unsigned long now, unsigned long duration_ms);
  void cancel() { until_ = 0; armed_ = false; }
  bool active(unsigned long now) const;
 private:
  unsigned long until_ = 0;
  bool armed_ = false;
};

// ---- spoof frame builder: frozen golden + patched fields + valid checksum ----
// out must hold 34 bytes. Units: V/A raw = tenth*10 (0.01 scale),
// temp raw = 2731 + tenth (JBD 0.1K offset), SOC direct byte.
void build_spoof_frame(const Bms2Config &cfg, uint8_t out[34]);
#define SPOOF_FRAME_LEN 34
