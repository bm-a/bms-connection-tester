// Virtual meter stimulus for Wokwi sim (meter MCU in diagram.json) — v1.1.
// Cycles registers 0x03/0x04/0x05 every 1s like a thorough real meter,
// prints whatever the DUT replies. DUT must answer all three (option A:
// silence only on writes/unknown). Observe DUT LEDs: green while polling,
// red if you stop this meter (reset it).
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
}
static uint8_t regs[] = {0x03, 0x04, 0x05};
static uint8_t idx = 0;
void loop() {
  uint8_t reg = regs[idx];
  idx = (idx + 1) % 3;
  uint16_t ck = 0x10000 - (reg + 0x00);
  uint8_t poll[7] = {0xDD, 0xA5, reg, 0x00, (uint8_t)(ck >> 8), (uint8_t)(ck & 0xFF), 0x77};
  Serial2.write(poll, 7);
  Serial2.flush();
  delay(200);
  Serial.print("meter reg ");
  Serial.print(reg, HEX);
  Serial.print(" rx: ");
  while (Serial2.available()) {
    char b[4];
    snprintf(b, sizeof(b), "%02X ", Serial2.read());
    Serial.print(b);
  }
  Serial.println();
  delay(800);
}
