// e-rickshaw meter RS485 connection tester — ESP32-S3 firmware v2.0.
// v1.x base FROZEN: JBD responder (0x03/0x04/05, option-A silence), adaptive
// link window, green/red LEDs + RGB mirror, STATUS?. v2.0 ADDS (never alters):
// 8-relay sequencer (sequential / all-ON, 3 button behaviors), always-on WiFi
// AP web UI (config + admin auth, NVS), spoof window (test values on 0x03).
//
// Wiring (ESP32-S3-DevKitC-1 / S3 N16R8):
//   GPIO17 (TX) -> MAX485 DI | GPIO16 (RX) -> MAX485 RO
//   GPIO4 -> MAX485 DE+RE tied (HIGH=TX, LOW=RX, 10k pull-down to GND)
//   GPIO10 -> green LED (+220R to GND) | GPIO11 -> red LED (+220R to GND)
//   GPIO48 -> onboard WS2812 RGB (mirrors green/red, no wiring needed)
//   GPIO5,6,7,8,9,12,13,14 -> relay module IN1..IN8 (12V coils, own supply,
//     common GND; default active-LOW, web toggle)
//   GPIO15 -> button to GND (press = LOW, internal pull-up; web-invertible)
//   GPIO21 -> spoof trigger to GND (internal pull-up; web-invertible)
//   Common GND. MAX485 VCC = 3.3V. USB powered (never the pack).
//
// USB-serial STATUS? extension (test jig only, NOT a JBD command):
//   "STATUS?\n" -> "GREEN 2.0\n" / "RED 2.0\n" (first token stable for HIL).

#include <Arduino.h>
#include "bms_protocol.h"
#include "relay_ctrl.h"
#include "web_ui.h"

#define PIN_RS485_TX   17
#define PIN_RS485_RX   16
#define PIN_RS485_DE    4
#define PIN_LED_GREEN  10
#define PIN_LED_RED    11
// Onboard WS2812 RGB (S3 N16R8 / DevKitC-1 with RGB). Mirrors the
// external LEDs so the box works with zero extra wiring.
// Built-in neopixelWrite() needs no extra library (Arduino-ESP32).
#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif
#define PIN_RGB RGB_BUILTIN
#define RGB_BRIGHT 32  // WS2812 is blinding at 255; 16-32 is plenty

#define PIN_BUTTON 15
#define PIN_SPOOF  21
static const uint8_t RELAY_PINS[RELAY_COUNT] = {5, 6, 7, 8, 9, 12, 13, 14};

#define EVAL_INTERVAL_MS 250UL  // brisk eval so fast/slow polls both feel live
#define FACTORY_RESET_HOLD_MS 10000UL  // button held 10 s -> wipe NVS + reboot

static JbdParser parser;
static PollTracker tracker;
static unsigned long lastEvalMs = 0;
static bool connected = false;

// ---- v2.0 state (base state above untouched) ----
static Bms2Config cfg;
static RelaySequencer seq;
static SpoofWindow spoof;
static DebouncedInput btn_in;
static DebouncedInput spoof_in;
static uint8_t spoofFrame[SPOOF_FRAME_LEN];
static bool spoofFrameReady = false;
static uint8_t lastRelayLevels[RELAY_COUNT];
static bool relaysArmed = false;
static unsigned long btnPressStart = 0;
static bool btnPressed = false;
static bool resetDone = false;

static void rebuild_spoof_frame() {
  build_spoof_frame(cfg, spoofFrame);
  spoofFrameReady = true;
}

static void apply_leds(bool on) {
  connected = on;
  digitalWrite(PIN_LED_GREEN, on ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, on ? LOW : HIGH);
  // Onboard RGB mirrors the discretes: green = talking, red = silent.
  // RMT-driven, safe to call from the 250 ms eval (never the hot RX loop).
#if defined(ARDUINO)
  neopixelWrite(PIN_RGB, on ? 0 : RGB_BRIGHT, on ? RGB_BRIGHT : 0, 0);
#endif
}

static void apply_relays() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    uint8_t lvl = relay_pin_level(seq.relayOn(i), cfg.active_low);
    if (!relaysArmed || lvl != lastRelayLevels[i]) {
      digitalWrite(RELAY_PINS[i], lvl);
      lastRelayLevels[i] = lvl;
    }
  }
  relaysArmed = true;
}

static void send_frame(const uint8_t *frame, size_t len) {
  digitalWrite(PIN_RS485_DE, HIGH); // TX mode
  Serial2.write(frame, len);
  Serial2.flush(true);              // wait TX complete, keep RX intact
  delayMicroseconds(1500);          // ~1.5 char guard @9600 before release
  digitalWrite(PIN_RS485_DE, LOW);  // back to RX
  while (Serial2.available()) Serial2.read();  // drop bytes sent while we TX'd
  parser.reset();                   // re-arm on the latest complete frame
}

static void handle_status_command() {
  static char line[16];
  static uint8_t pos = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line[pos] = '\0';
      if (strcmp(line, "STATUS?") == 0) {
        Serial.print(connected ? "GREEN " : "RED ");
        Serial.println(FW_VERSION);
      }
      pos = 0;
    } else if (pos < sizeof(line) - 1) {
      line[pos++] = c;
    } else {
      pos = 0; // overflow: reset line
    }
  }
}

void setup() {
  pinMode(PIN_RS485_DE, OUTPUT);
  digitalWrite(PIN_RS485_DE, LOW); // RX mode first (with 10k pull-down in HW)
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  apply_leds(false); // boot red

  // ---- v2.0 init (after frozen LED boot state) ----
  seq.begin(&cfg);
  btn_in.begin(true);
  spoof_in.begin(true);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SPOOF, INPUT_PULLUP);
  static WebCtx wctx;
  wctx.cfg = &cfg;
  wctx.seq = &seq;
  wctx.spoof = &spoof;
  wctx.link_green = &connected;
  wctx.on_config_changed = rebuild_spoof_frame;
  web_setup(wctx);  // loads NVS config, builds spoof frame, starts always-on AP
  // Relays: drive OFF level BEFORE pinMode so nothing clicks at boot.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    uint8_t off = relay_pin_level(false, cfg.active_low);
    digitalWrite(RELAY_PINS[i], off);
    pinMode(RELAY_PINS[i], OUTPUT);
    lastRelayLevels[i] = off;
  }
  relaysArmed = true;

  Serial.begin(115200);
  // Don't block headless boot waiting for USB console.
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { delay(10); }

  Serial2.begin(9600, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  // Discard any boot garbage on the bus.
  while (Serial2.available()) Serial2.read();
  lastEvalMs = millis();
}

void loop() {
  unsigned long now = millis();

  // --- v2.0: button (press event + 10 s long-press factory reset) ---
  bool btnRaw = digitalRead(PIN_BUTTON) == HIGH;  // pull-up idle HIGH
  bool pressed = cfg.button_invert ? btnRaw : !btnRaw;
  btn_in.update(!pressed, now);  // feed idle-HIGH domain: fell = press
  if (btn_in.fell()) {
    btnPressed = true;
    btnPressStart = now;
    handle_button_press(seq, cfg, now);
  }
  if (btn_in.rose()) btnPressed = false;
  if (btnPressed && !resetDone &&
      (now - btnPressStart) >= FACTORY_RESET_HOLD_MS) {
    resetDone = true;
    web_factory_reset();  // wipes NVS + reboots; never returns
  }

  // --- v2.0: spoof trigger input ---
  bool spRaw = digitalRead(PIN_SPOOF) == HIGH;
  bool spActive = cfg.spoof_invert ? spRaw : !spRaw;
  spoof_in.update(!spActive, now);
  if (spoof_in.fell() && cfg.spoof_enabled)
    spoof.trigger(now, (unsigned long)cfg.spoof_seconds * 1000UL);

  // --- v2.0: sequencer + relay outputs + web server (all non-blocking) ---
  seq.tick(now);
  apply_relays();
  web_tick(now);

  // --- FROZEN v1.x RS485 path (untouched logic) ---
  JbdFrame f;
  while (Serial2.available()) {
    if (parser.feed((uint8_t)Serial2.read(), f)) {
      // Any well-formed meter frame proves wiring: refresh green window.
      tracker.note_poll(now);
      // v2.0: during a spoof window, reg 0x03 answers test values; every
      // other rule (option-A silence, golden/cell/name frames) is frozen.
      size_t rlen = 0;
      const uint8_t *reply = reply_for(f.reg, f.is_write, rlen);
      if (!f.is_write && f.reg == 0x03 && cfg.spoof_enabled &&
          spoofFrameReady && spoof.active(now)) {
        reply = spoofFrame;
        rlen = SPOOF_FRAME_LEN;
      }
      if (reply) send_frame(reply, rlen);
    }
  }

  // --- live status evaluation (millis timer, no delay) ---
  if (now - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs = now;
    apply_leds(tracker.active(now));
  }

  handle_status_command();
}
