// e-rickshaw meter RS485 connection tester — Arduino IDE sketch.
// Board: "ESP32S3 Dev Module" | USB CDC On Boot: Enabled | Upload Speed: 921600
//
// WIRING (ESP32-S3 DevKitC-1):
//   GPIO17 (TX) -> MAX485 DI | GPIO16 (RX) -> MAX485 RO
//   GPIO4 -> MAX485 DE+RE tied (HIGH=TX, LOW=RX) + 10k pull-down to GND
//   GPIO10 -> 220R -> GREEN LED -> GND | GPIO11 -> 220R -> RED LED -> GND
//   MAX485 VCC=3.3V, common GND, A/B -> meter (twisted pair).
//
// BEHAVIOR: green = meter polling seen within last 2s, red = not.
// Nothing to press, no reset. STATUS? on USB serial answers GREEN/RED (test jig).

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
  digitalWrite(PIN_RS485_DE, HIGH);
  Serial2.write(BMS_RESPONSE, BMS_RESPONSE_LEN);
  Serial2.flush(true);
  delayMicroseconds(1500); // ~1.5 char guard @9600 before releasing bus
  digitalWrite(PIN_RS485_DE, LOW);
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
      pos = 0;
    }
  }
}

void setup() {
  pinMode(PIN_RS485_DE, OUTPUT);
  digitalWrite(PIN_RS485_DE, LOW);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  apply_leds(false); // boot red

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { delay(10); } // don't block headless boot

  Serial2.begin(9600, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  while (Serial2.available()) Serial2.read(); // discard boot garbage
  lastEvalMs = millis();
}

void loop() {
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
      rxCount = 0;
    }
  }

  unsigned long now = millis();
  if (now - lastEvalMs >= EVAL_INTERVAL_MS) {
    lastEvalMs = now;
    apply_leds(connection_active(now, lastValidRequestMs));
  }

  handle_status_command();
}
