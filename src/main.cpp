// e-rickshaw meter RS485 connection tester — ESP32-S3 firmware v1.1.
// Listens for the meter's JBD polls (any register) and replies with canned
// frames for 0x03/0x04/0x05; silent on writes/unknown (option A).
// Green LED = meter talking (adaptive window), red = not.
// No buttons, no latch, no display.
//
// Wiring (ESP32-S3-DevKitC-1):
//   GPIO17 (TX) -> MAX485 DI | GPIO16 (RX) -> MAX485 RO
//   GPIO4 -> MAX485 DE+RE tied (HIGH=TX, LOW=RX, 10k pull-down to GND)
//   GPIO10 -> green LED (+220R to GND) | GPIO11 -> red LED (+220R to GND)
//   Common GND. MAX485 VCC = 3.3V. USB powered (never the pack).
//
// USB-serial STATUS? extension (test jig only, NOT a JBD command):
//   "STATUS?\n" -> "GREEN 1.1\n" / "RED 1.1\n" (first token stable for HIL).

#include <Arduino.h>
#include "bms_protocol.h"

#define PIN_RS485_TX   17
#define PIN_RS485_RX   16
#define PIN_RS485_DE    4
#define PIN_LED_GREEN  10
#define PIN_LED_RED    11

#define EVAL_INTERVAL_MS 250UL  // brisk eval so fast/slow polls both feel live

static JbdParser parser;
static PollTracker tracker;
static unsigned long lastEvalMs = 0;
static bool connected = false;

static void apply_leds(bool on) {
  connected = on;
  digitalWrite(PIN_LED_GREEN, on ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, on ? LOW : HIGH);
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
  // --- non-blocking RS485 RX through the validating parser ---
  JbdFrame f;
  while (Serial2.available()) {
    if (parser.feed((uint8_t)Serial2.read(), f)) {
      // Any well-formed meter frame proves wiring: refresh green window.
      tracker.note_poll(millis());
      // Answer only known reads (option A: silence otherwise).
      size_t rlen = 0;
      const uint8_t *reply = reply_for(f.reg, f.is_write, rlen);
      if (reply) send_frame(reply, rlen);
    }
  }

  // --- live status evaluation (millis timer, no delay) ---
  unsigned long now = millis();
  if (now - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs = now;
    apply_leds(tracker.active(now));
  }

  handle_status_command();
}
