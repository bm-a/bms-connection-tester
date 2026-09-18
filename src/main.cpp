// e-rickshaw meter RS485 connection tester — ESP32-S3 firmware.
// Listens for the meter's fixed JBD poll (DD A5 03 00 FF FD 77) and
// replies with one canned SOC-100% frame. Green LED = polling seen
// within last 2s, red LED = not. No buttons, no latch, no display.
//
// Wiring (ESP32-S3-DevKitC-1):
//   GPIO17 (TX) -> MAX485 DI | GPIO16 (RX) -> MAX485 RO
//   GPIO4 -> MAX485 DE+RE tied (HIGH=TX, LOW=RX, 10k pull-down to GND)
//   GPIO10 -> green LED (+220R to GND) | GPIO11 -> red LED (+220R to GND)
//   Common GND everywhere. MAX485 VCC = 3.3V. USB powered (never the pack).
//
// USB-serial STATUS? extension (NOT a JBD command, test jig only):
//   PC sends "STATUS?\n" on Serial (UART0 console) -> replies "GREEN\n"/"RED\n".

#include <Arduino.h>
#include "bms_protocol.h"

#define PIN_RS485_TX   17
#define PIN_RS485_RX   16
#define PIN_RS485_DE    4
#define PIN_LED_GREEN  10
#define PIN_LED_RED    11

#define EVAL_INTERVAL_MS 1000UL

static uint8_t rxWindow[BMS_REQUEST_LEN];
static uint8_t rxCount = 0;
static unsigned long lastValidRequestMs = 0; // 0 => boots red
static unsigned long lastEvalMs = 0;
static bool connected = false;

static void apply_leds(bool on) {
  connected = on;
  digitalWrite(PIN_LED_GREEN, on ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, on ? LOW : HIGH);
}

static void send_canned_response() {
  digitalWrite(PIN_RS485_DE, HIGH); // TX mode
  Serial2.write(BMS_RESPONSE, BMS_RESPONSE_LEN);
  Serial2.flush(true);              // wait TX complete, keep RX intact
  delayMicroseconds(1500);          // ~1.5 char guard @9600 before release
  digitalWrite(PIN_RS485_DE, LOW);  // back to RX
}

static void handle_status_command() {
  static char line[16];
  static uint8_t pos = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line[pos] = '\0';
      if (strcmp(line, "STATUS?") == 0) {
        Serial.println(connected ? "GREEN" : "RED");
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
  // --- non-blocking RS485 RX into a rolling 7-byte window ---
  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();
    if (rxCount < BMS_REQUEST_LEN) {
      rxWindow[rxCount++] = b;
    } else {
      memmove(rxWindow, rxWindow + 1, BMS_REQUEST_LEN - 1);
      rxWindow[BMS_REQUEST_LEN - 1] = b;
    }
    if (rxCount == BMS_REQUEST_LEN && matches_request(rxWindow)) {
      send_canned_response();
      lastValidRequestMs = millis();
      rxCount = 0; // re-arm: prevents double-trigger on same bytes
    }
  }

  // --- 1s live status evaluation (millis timer, no delay) ---
  unsigned long now = millis();
  if (now - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs = now;
    apply_leds(connection_active(now, lastValidRequestMs));
  }

  handle_status_command();
}
