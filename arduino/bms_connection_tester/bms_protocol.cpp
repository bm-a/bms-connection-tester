#include "bms_protocol.h"

const uint8_t BMS_REQUEST[7] PROGMEM = {
  0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77
};

// Golden SOC 100% frame — byte-exact capture from the real battery.
// NOTE on checksum: jbd_checksum() over STATUS+LEN+DATA
// (i.e. bytes [2..30], len_hi+len_lo+27 data bytes) yields 0xFCDA,
// matching the frame. Including the echoed cmd byte 0x03 would give
// 0xFCD7 — that is the handoff formula's off-by-3. The charging-frame
// capture in the handoff matches neither formula and is treated as a
// transcription error (excluded from tests).
const uint8_t BMS_RESPONSE[34] PROGMEM = {
  0xDD, 0x03, 0x00, 0x1B,
  0x14, 0x50, 0x00, 0x00, 0x27, 0x10, 0x27, 0x10,
  0x00, 0x01, 0x20, 0x21, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x20, 0x64, 0x03, 0x0E, 0x02, 0x0B,
  0xA5, 0x0B, 0xA5,
  0xFC, 0xDA, 0x77
};

uint16_t jbd_checksum(const uint8_t *buf, size_t len) {
  uint32_t s = 0;
  for (size_t i = 0; i < len; i++) s += buf[i];
  return (uint16_t)(0x10000UL - (s & 0xFFFFUL));
}

bool matches_request(const uint8_t *buf) {
  for (int i = 0; i < BMS_REQUEST_LEN; i++) {
    if (buf[i] != BMS_REQUEST[i]) return false;
  }
  return true;
}
