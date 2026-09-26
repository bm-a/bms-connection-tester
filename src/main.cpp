// e-rickshaw meter RS485 connection tester — ESP32-S3 firmware v2.7.
// v1.x base FROZEN: JBD responder (0x03/0x04/05, option-A silence), adaptive
// link window, green/red LEDs + RGB mirror, STATUS?. v2.0 ADDS (never alters):
// 8-relay sequencer (sequential / all-ON, 3 button behaviors), always-on WiFi
// AP web UI (config + admin auth, NVS), spoof window (test values on 0x03).
// v2.3 ADDS: relay count (first N) + chase-wave mode, 2-stage spoof
// (100-first then 88.8/188, both stages editable, timings per stage),
// hold in milliseconds, manual + automatic OTA.
// v2.4 ADDS: Tasmota-grade update gates, per-mode relay menu (chase BBM +
// stop dead-band + new timing floors), spoof save-only, console, config
// backup/restore, custom OTA URL, STA uplink test, info card, mDNS.
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
//   GPIO18 -> WiFi kill to GND (ground = AP off; internal pull-up)
//   Common GND. MAX485 VCC = 3.3V. USB powered (never the pack).
//
// Wiring (Waveshare ESP32-S3-ETH-8DI-8RO, -DBOARD_WAVESHARE_8DI8RO=1):
//   Relays R1..R8 -> TCA9554PWR EXIO1..EXIO8 @ I2C 0x20 (SDA42/SCL41),
//     output-register bit i = relay i+1, HIGH bit = ON (fixed in hardware;
//     the web "active-low" toggle is a documented no-op on this board)
//   GPIO17 (TX) -> onboard isolated RS485 (SP3485, hardware auto direction)
//   GPIO18 (RX) -> onboard isolated RS485 (no DE/RE pin exists)
//   GPIO0 (BOOT button) -> START/STOP to GND (press = LOW, internal pull-up)
//   GPIO4 (DI1 terminal) -> spoof trigger to COM (active = LOW, web-changeable)
//   GPIO5 (DI2 terminal) -> WiFi kill to COM (active = LOW = AP off)
//   GPIO38 -> onboard WS2812 RGB (mirrors green/red, no wiring needed)
//   GPIO12..16 = W5500 Ethernet (reserved, untouched) | GPIO46 = buzzer
//   DI3..DI8 (GPIO6..11) free. USB-C powered (never the pack).
//
// USB-serial STATUS? extension (test jig only, NOT a JBD command):
//   "STATUS?\n" -> "GREEN 2.6\n" / "RED 2.6\n" (first token stable for HIL).

#include <Arduino.h>
#include "bms_protocol.h"
#include "fw_upload.h"  // v2.4 shared update gates (host-tested)
#include "relay_ctrl.h"
#include "web_ui.h"
#include "ota.h"
#ifdef BOARD_WAVESHARE_8DI8RO
#include "waveshare_pins.h"  // TCA9554 relay map + pure port logic (host-tested)
#include <Wire.h>
#endif
#define RS485_SERIAL Serial2
// OTA transport (ESP-only; never host-built — main.cpp is the sketch).
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

#ifdef BOARD_WAVESHARE_8DI8RO
// Waveshare ESP32-S3-ETH-8DI-8RO: relays live on the TCA9554PWR I2C expander
// (see waveshare_pins.h), RS485 is TX17/RX18 with hardware auto direction
// (no DE pin), BOOT = START/STOP, DI1/DI2 = spoof/WiFi-kill, RGB on GPIO38.
#define PIN_RS485_TX   WS_PIN_RS485_TX
#define PIN_RS485_RX   WS_PIN_RS485_RX
// (no PIN_RS485_DE — the SP3485 switches direction in hardware)
// (no discrete green/red LEDs — RGB only)
#define PIN_RGB WS_PIN_RGB  // onboard WS2812; not RGB_BUILTIN (core boards
                            // vary) — fixed 38 per Waveshare schematic
#define RGB_BRIGHT 32  // WS2812 is blinding at 255; 16-32 is plenty

#define PIN_BUTTON WS_PIN_BUTTON
#define PIN_SPOOF  WS_PIN_SPOOF
#define PIN_WIFI_KILL WS_PIN_WIFI_KILL
// (no RELAY_PINS — relays are TCA9554 output-register bits)
#else
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
// v2.3.1 WiFi kill switch (manual, default ON): free DIO, non-strapping,
// no ADC/boot role. Ground to kill the AP, release to bring it back.
#define PIN_WIFI_KILL 18
static const uint8_t RELAY_PINS[RELAY_COUNT] = {5, 6, 7, 8, 9, 12, 13, 14};
#endif

#define EVAL_INTERVAL_MS 250UL  // brisk eval so fast/slow polls both feel live
#define FACTORY_RESET_HOLD_MS 10000UL  // button held 10 s -> wipe NVS + reboot

static JbdParser parser;
static PollTracker tracker;
static unsigned long lastEvalMs = 0;
static bool connected = false;

// ---- v2.0 state (base state above untouched) ----
static Bms2Config cfg;
static RelaySequencer seq;
static SpoofPlan spoof;  // v2.3: two-stage (100 first, then 88.8/188)
static DebouncedInput btn_in;
static DebouncedInput spoof_in;
static DebouncedInput wifi_in;  // v2.3.1 AP kill switch (ground = WiFi off)
#ifdef BOARD_WAVESHARE_8DI8RO
static uint8_t curSpoofPin = WS_PIN_SPOOF;  // v2.3.1: follows cfg.spoof_pin
#else
static uint8_t curSpoofPin = 21;  // v2.3.1: follows cfg.spoof_pin (sanitized)
#endif
static uint8_t spoofFrameA[SPOOF_FRAME_LEN];  // stage 1 ("100")
static uint8_t spoofFrameB[SPOOF_FRAME_LEN];  // stage 2 (88.8/188 pattern)
static bool spoofFrameReady = false;
static uint8_t lastRelayLevels[RELAY_COUNT];
static bool relaysArmed = false;
#ifdef BOARD_WAVESHARE_8DI8RO
static uint8_t lastWsRelayBits = 0x00;  // TCA9554 output-register cache

// Minimal TCA9554PWR access: single-register write. Returns false on bus
// error (caller parks safe / retries; sequencer state is untouched).
static bool tca_write_reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(WS_TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// All pins output, all relays OFF — the expander equivalent of the
// direct-GPIO "drive OFF level BEFORE pinMode" rule.
static bool tca_relays_init() {
  // Safe order: park outputs OFF first, then configure pins as outputs.
  return tca_write_reg(WS_TCA9554_REG_OUTPUT, 0x00) &&
         tca_write_reg(WS_TCA9554_REG_CONFIG, 0x00);
}
#endif
static unsigned long btnPressStart = 0;
static bool btnPressed = false;
static bool resetDone = false;
static unsigned long last_bus_ms = 0;  // v2.3: last valid meter frame (OTA gate)

static void rebuild_spoof_frame() {
  build_spoof_frame(cfg, 1, spoofFrameA);
  build_spoof_frame(cfg, 2, spoofFrameB);
  spoofFrameReady = true;
}

static void apply_leds(bool on) {
  connected = on;
#ifdef BOARD_WAVESHARE_8DI8RO
  // No discrete LEDs on this board — the onboard RGB (GPIO38) is the lamp.
#else
  digitalWrite(PIN_LED_GREEN, on ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, on ? LOW : HIGH);
#endif
  // Onboard RGB mirrors the discretes: green = talking, red = silent.
  // RMT-driven, safe to call from the 250 ms eval (never the hot RX loop).
#if defined(ARDUINO)
  neopixelWrite(PIN_RGB, on ? 0 : RGB_BRIGHT, on ? RGB_BRIGHT : 0, 0);
#endif
}

static void apply_relays() {
  // v2.3: only the first cfg.relay_count relays participate; pins beyond
  // the count are forced OFF so a shrunk count can't leave a coil latched.
  uint8_t n = cfg.relay_count;
  if (n < 1 || n > RELAY_COUNT) n = RELAY_COUNT;
#ifdef BOARD_WAVESHARE_8DI8RO
  // TCA9554PWR: one I2C write carries all 8 relays (bit i = relay i+1,
  // HIGH = ON). cfg.active_low is fixed hardware here and intentionally
  // ignored — dashboard labels always match the coils. Change detection
  // via the output-register cache, same as the GPIO path below.
  bool on[RELAY_COUNT];
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    on[i] = (i < n) ? seq.relayOn(i) : false;
  uint8_t bits = ws_output_byte(on, n);
  if (!relaysArmed || bits != lastWsRelayBits) {
    bool ok = tca_write_reg(WS_TCA9554_REG_OUTPUT, bits);
    if (ok) {
      lastWsRelayBits = bits;
    } else if (bits != 0x00) {
      tca_write_reg(WS_TCA9554_REG_OUTPUT, 0x00);  // park safe on bus error
      // lastWsRelayBits untouched -> the intended state retries next eval
    }
  }
  relaysArmed = true;
#else
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    bool on = (i < n) ? seq.relayOn(i) : false;
    uint8_t lvl = relay_pin_level(on, cfg.active_low);
    if (!relaysArmed || lvl != lastRelayLevels[i]) {
      digitalWrite(RELAY_PINS[i], lvl);
      lastRelayLevels[i] = lvl;
    }
  }
  relaysArmed = true;
#endif
}

// ---- v2.3 OTA (GitHub releases; needs the optional STA uplink) ----
static OtaState ota;

static void ota_set_status(const char *s) {
  strncpy(ota.status, s, sizeof(ota.status) - 1);
  ota.status[sizeof(ota.status) - 1] = '\0';
}

// Query the latest release tag from GitHub. Caller guarantees STA online.
static void ota_check_now() {
  unsigned long now = millis();
  ota.last_check_ms = now;
  WiFiClientSecure cli;
  cli.setInsecure();  // LAN bench box; a bad flash is fixed over USB
  HTTPClient http;
#ifdef BOARD_WAVESHARE_8DI8RO
  // This board runs its own prerelease line (tags v2.7-wsN), which GitHub
  // never reports as /releases/latest — scan the newest releases instead.
  const char *releases_url =
      "https://api.github.com/repos/bm-a/bms-connection-tester/"
      "releases?per_page=20";
#else
  const char *releases_url =
      "https://api.github.com/repos/bm-a/bms-connection-tester/"
      "releases/latest";
#endif
  if (!http.begin(cli, releases_url)) {
    ota_set_status("check failed");
    return;
  }
  int code = http.GET();
  if (code != 200) {
    char b[48];
    snprintf(b, sizeof(b), "check http %d", code);
    ota_set_status(b);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();
  // GitHub pretty-prints ("tag_name": "v2.3"); tolerate any gap after ':'.
  // The Waveshare build scans every tag_name in the release list (newest
  // first) for its own v2.7-wsN line; other builds take the first tag.
  String tag;
  int from = 0;
  for (;;) {
    int ti = body.indexOf("\"tag_name\"", (unsigned)from);
    if (ti < 0) break;
    int ci = body.indexOf(':', (unsigned)(ti + 10));
    if (ci < 0) break;
    int q1 = body.indexOf('"', (unsigned)(ci + 1));
    if (q1 < 0) break;
    String cand = body.substring((unsigned)(q1 + 1));
    int q = cand.indexOf('"');
    if (q >= 0) cand = cand.substring(0, (unsigned)q);
    from = q1 + 1;
    if (cand.length() == 0 || cand.length() >= (int)sizeof(ota.latest_tag))
      continue;  // malformed entry: skip it, keep scanning
#ifdef BOARD_WAVESHARE_8DI8RO
    // Own prerelease line: keep scanning and keep the HIGHEST valid -wsN
    // version (GitHub order is by creation date, not version).
    if (ota_tag_is_waveshare_line(cand.c_str()) &&
        (tag.length() == 0 ||
         ota_cmp_version(cand.c_str(), tag.c_str()) > 0)) {
      tag = cand;
    }
#else
    tag = cand;
    break;
#endif
  }
  if (tag.length() == 0) {
#ifdef BOARD_WAVESHARE_8DI8RO
    ota_set_status("no ws release");
#else
    ota_set_status("bad api reply");
#endif
    return;
  }
  strncpy(ota.latest_tag, tag.c_str(), sizeof(ota.latest_tag) - 1);
  ota.update_pending = ota_cmp_version(tag.c_str(), FW_VERSION) > 0;
  ota_set_status(ota.update_pending ? "update available" : "up to date");
}

// Download + install the pending release (or the custom URL from the
// dashboard). Streams the asset straight into flash (no 700 KB RAM copy —
// the 8 MB board has no PSRAM) under the SAME Tasmota-grade gates as the
// manual upload: exact variant asset, explicit budget, image-head check.
// Reboots on success; returns only on failure (old firmware keeps running).
static void ota_install_now() {
  char url[160];
  if (web_ota_url()[0] != '\0') {
    // Custom URL (dashboard-validated http(s) .bin on save; re-checked).
    if (strncmp(web_ota_url(), "http", 4) != 0) {
      ota_set_status("bad custom url");
      return;
    }
    strncpy(url, web_ota_url(), sizeof(url) - 1);
    url[sizeof(url) - 1] = '\0';
    // The custom file must still be this variant's asset (no cross-flash).
    const char *tail = url;
    for (const char *p = url; *p; p++) {
      if (*p == '/') tail = p + 1;
    }
    if (!fw_filename_ok(tail, FW_VARIANT)) {
      ota_set_status(fw_err_str(FW_WRONG_FILE));
      return;
    }
  } else if (!ota_download_url(ota.latest_tag, FW_VARIANT, url,
                               sizeof(url))) {
    ota_set_status("bad url");
    return;
  }
  ota_set_status("installing...");
  WiFiClientSecure cli;
  cli.setInsecure();  // LAN bench box; a bad flash is fixed over USB
  HTTPClient http;
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // github redirect
  http.setTimeout(30000);
  if (!http.begin(cli, url)) {
    ota_set_status("install begin failed");
    return;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char b[48];
    snprintf(b, sizeof(b), "install http %d", code);
    ota_set_status(b);
    http.end();
    return;
  }
  int len = http.getSize();
  uint32_t max_space =
      fw_max_sketch_space((uint32_t)ESP.getFreeSketchSpace());
  if (!fw_size_ok(len > 0 ? (uint32_t)len : 0, max_space)) {
    ota_set_status(fw_err_str(FW_TOO_BIG));
    http.end();
    return;
  }
  if (!Update.begin(max_space)) {
    ota_set_status(fw_err_str(FW_BEGIN_FAIL));
    http.end();
    return;
  }
  // Gate the stream head before committing (read 4, check, write 4, rest).
  WiFiClient &stream = http.getStream();
  uint8_t head[4];
  size_t hn = 0;
  while (hn < 4) {
    int c = stream.read();
    if (c < 0) break;
    head[hn++] = (uint8_t)c;
  }
  FwUploadErr gg =
      fw_gate_image_head(head, hn, (uint32_t)ESP.getFlashChipSize());
  if (gg != FW_OK) {
    ota_set_status(fw_err_str(gg));
    Update.end(false);
    http.end();
    return;
  }
  if (Update.write(head, hn) != hn) {
    ota_set_status(fw_err_str(FW_WRITE_FAIL));
    Update.end(false);
    http.end();
    return;
  }
  size_t written = hn + Update.writeStream(stream);
  http.end();
  if (written == 0 || Update.hasError() || !Update.end(true)) {
    ota_set_status("install failed");
    return;
  }
  ota_set_status("rebooting...");
  delay(500);
  ESP.restart();
}

static void ota_auto_tick(unsigned long now) {
  if (!ota_should_check(now, ota.last_check_ms, ota.interval_ms,
                        ota.auto_enabled, web_sta_state() == 2, seq.running(),
                        now - last_bus_ms, 60000UL))
    return;
  ota_check_now();
  if (ota.update_pending) ota_install_now();  // bus proven silent by the gate
}

static void send_frame(const uint8_t *frame, size_t len) {
#ifdef BOARD_WAVESHARE_8DI8RO
  // Onboard SP3485 switches direction in hardware — no DE pin to drive.
#else
  digitalWrite(PIN_RS485_DE, HIGH); // TX mode
#endif
  RS485_SERIAL.write(frame, len);
  RS485_SERIAL.flush(true);              // wait TX complete, keep RX intact
  delayMicroseconds(1500);          // ~1.5 char guard @9600 before release
#ifndef BOARD_WAVESHARE_8DI8RO
  digitalWrite(PIN_RS485_DE, LOW);  // back to RX
#endif
  while (RS485_SERIAL.available()) RS485_SERIAL.read();  // drop bytes sent while we TX'd
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
#ifdef BOARD_WAVESHARE_8DI8RO
  Wire.begin(WS_I2C_SDA, WS_I2C_SCL);
  tca_relays_init();  // expander: all outputs, all relays OFF first
#else
  pinMode(PIN_RS485_DE, OUTPUT);
  digitalWrite(PIN_RS485_DE, LOW); // RX mode first (with 10k pull-down in HW)
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
#endif
  apply_leds(false); // boot red

  // ---- v2.0 init (after frozen LED boot state) ----
  seq.begin(&cfg);
  btn_in.begin(true);
  spoof_in.begin(true);
  wifi_in.begin(true);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SPOOF, INPUT_PULLUP);
  pinMode(PIN_WIFI_KILL, INPUT_PULLUP);  // idle HIGH = AP on
  static WebCtx wctx;
  wctx.cfg = &cfg;
  wctx.seq = &seq;
  wctx.spoof = &spoof;
  wctx.link_green = &connected;
  wctx.on_config_changed = rebuild_spoof_frame;
  wctx.ota = &ota;
  wctx.on_ota_check = ota_check_now;
  wctx.on_ota_install = ota_install_now;
  web_setup(wctx);  // loads NVS config, builds spoof frames, starts always-on AP
  // v2.3 burn-in: auto-start the configured mode (relays already OFF-first).
  // (void): boot start is always outside the post-stop dead-band (R12).
  if (cfg.boot_autostart) (void)seq.start(millis());
#ifdef BOARD_WAVESHARE_8DI8RO
  // Relays already parked OFF by tca_relays_init() above; seed the cache so
  // the first apply_relays() only writes on a real state change.
  lastWsRelayBits = 0x00;
#else
  // Relays: drive OFF level BEFORE pinMode so nothing clicks at boot.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    uint8_t off = relay_pin_level(false, cfg.active_low);
    digitalWrite(RELAY_PINS[i], off);
    pinMode(RELAY_PINS[i], OUTPUT);
    lastRelayLevels[i] = off;
  }
#endif
  relaysArmed = true;
  Serial.begin(115200);
  // Don't block headless boot waiting for USB console.
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { delay(10); }

  RS485_SERIAL.begin(9600, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  // Discard any boot garbage on the bus.
  while (RS485_SERIAL.available()) RS485_SERIAL.read();
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

  // --- v2.3.1: WiFi kill switch (ground PIN_WIFI_KILL = AP off now) ---
  bool wifiRaw = digitalRead(PIN_WIFI_KILL) == HIGH;  // pull-up idle HIGH
  wifi_in.update(wifiRaw, now);  // fell = grounded, rose = released
  if (wifi_in.fell()) web_wifi_set(false);
  else if (wifi_in.rose()) web_wifi_set(true);

  // --- v2.0: spoof trigger input (v2.3: fires the two-stage plan) ---
  // v2.3.1: pin follows cfg.spoof_pin (NVS, sanitized); re-arm on change.
  uint8_t wantPin = sanitize_spoof_pin(cfg.spoof_pin);
  if (wantPin != curSpoofPin) {
    pinMode(curSpoofPin, INPUT);  // release the old pin (no pull)
    curSpoofPin = wantPin;
    cfg.spoof_pin = wantPin;
    pinMode(curSpoofPin, INPUT_PULLUP);
    spoof_in.begin(true);
  }
  bool spRaw = digitalRead(curSpoofPin) == HIGH;
  bool spActive = cfg.spoof_invert ? spRaw : !spRaw;
  spoof_in.update(!spActive, now);
  if (spoof_in.fell() && cfg.spoof_enabled)
    spoof.trigger(now, (unsigned long)cfg.spoof_seconds * 1000UL,
                  (unsigned long)cfg.s2_seconds * 1000UL);

  // --- v2.0: sequencer + relay outputs + web server (all non-blocking) ---
  // (v2.6 meter heuristic is fed inside web_tick(): link gaps = new meter.)
  seq.tick(now);
  apply_relays();
  web_tick(now);
  ota_auto_tick(now);  // v2.3: no-op without STA internet + idle bench

  // --- FROZEN v1.x RS485 path (untouched logic) ---
  JbdFrame f;
  while (RS485_SERIAL.available()) {
    if (parser.feed((uint8_t)RS485_SERIAL.read(), f)) {
      // Any well-formed meter frame proves wiring: refresh green window.
      tracker.note_poll(now);
      last_bus_ms = now;
      // v2.3: during a spoof plan, reg 0x03 answers stage 1 ("100") then
      // stage 2 (88.8/188); every other rule (option-A silence,
      // golden/cell/name frames) is frozen. The rule itself lives in
      // select_reply() (host-tested in test_system).
      size_t rlen = 0;
      uint8_t stage = spoof.stage(now);
      const uint8_t *sf = nullptr;
      if (stage == 2) sf = spoofFrameB;
      else if (stage == 1) sf = spoofFrameA;
      const uint8_t *reply = select_reply(
          f, cfg, stage, (sf && spoofFrameReady) ? sf : nullptr, rlen);
      if (reply) send_frame(reply, rlen);
    }
  }

  // --- live status evaluation (millis timer, no delay) ---
  if (now - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs = now;
    apply_leds(tracker.active(now));
  }
  // (v2.6 meter heuristic is fed inside web_tick() below: link gaps = new
  // meter. One funnel — main.cpp never touches the batch directly.)

  handle_status_command();
}
