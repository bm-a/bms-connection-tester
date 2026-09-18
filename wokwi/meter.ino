// Virtual meter stimulus for Wokwi sim (meter MCU in diagram.json).
// Sends the fixed JBD poll every 1s on Serial2 (pins 16 RX / 17 TX),
// prints whatever the DUT replies. Observe DUT LEDs: green while
// polling, red if you stop this meter (reset it).
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
}
const uint8_t POLL[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
void loop() {
  Serial2.write(POLL, 7);
  Serial2.flush();
  delay(200);
  Serial.print("meter rx: ");
  while (Serial2.available()) {
    char b[4];
    snprintf(b, sizeof(b), "%02X ", Serial2.read());
    Serial.print(b);
  }
  Serial.println();
  delay(800);
}
