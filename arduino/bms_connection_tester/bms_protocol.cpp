#include "bms_protocol.h"

const uint8_t BMS_REQUEST[7] PROGMEM = {
  0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77
};

// Golden SOC 100% frame — byte-exact capture from the real battery.
// Checksum FC DA covers STATUS+LEN+DATA (bytes [2..30]).
const uint8_t BMS_RESPONSE[34] PROGMEM = {
  0xDD, 0x03, 0x00, 0x1B,
  0x14, 0x50, 0x00, 0x00, 0x27, 0x10, 0x27, 0x10,
  0x00, 0x01, 0x20, 0x21, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x20, 0x64, 0x03, 0x0E, 0x02, 0x0B,
  0xA5, 0x0B, 0xA5,
  0xFC, 0xDA, 0x77
};

// Synthesized 14S cell voltages, consistent with the golden frame:
// 14 x 0x0E82 (3714 mV) = 51996 mV = 52.0 V. Checksum F8 04 covers
// LEN_HI+LEN_LO+28 data bytes (same verified rule as the captures).
const uint8_t BMS_RESPONSE_CELLS[35] PROGMEM = {
  0xDD, 0x04, 0x00, 0x1C,
  0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82,
  0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82,
  0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82, 0x0E, 0x82,
  0x0E, 0x82, 0x0E, 0x82,
  0xF8, 0x04, 0x77
};

// Synthesized device name "TEST-14S100A". Checksum FC FD covers
// LEN_HI+LEN_LO+12 name bytes.
const uint8_t BMS_RESPONSE_NAME[19] PROGMEM = {
  0xDD, 0x05, 0x00, 0x0C,
  0x54, 0x45, 0x53, 0x54, 0x2D, 0x31, 0x34, 0x53,
  0x31, 0x30, 0x30, 0x41,
  0xFC, 0xFD, 0x77
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

void JbdParser::reset() {
  st = JST_HUNT;
  is_write = false;
  ck_ok = false;
  reg = len = ckhi = 0;
  got = 0;
}

void JbdParser::restart() {
  // Fresh candidate starting at a DD byte (already consumed).
  st = JST_B1;
  is_write = false;
  ck_ok = false;
  reg = len = ckhi = 0;
  got = 0;
}

bool JbdParser::feed(uint8_t b, JbdFrame &out) {
  switch (st) {
    case JST_HUNT:
      if (b == 0xDD) restart();
      return false;
    case JST_B1:
      if (b == 0xA5) { is_write = false; st = JST_REG; }
      else if (b == 0x5A) { is_write = true; st = JST_REG; }
      else if (b == 0xDD) restart();  // re-sync: DD DD ... starts over
      else reset();
      return false;
    case JST_REG:
      reg = b;
      st = JST_LEN;
      return false;
    case JST_LEN:
      if (b > JBD_MAX_DATA) {
        // Overlong: cannot be ours. Re-sync if this byte is a DD.
        if (b == 0xDD) restart();
        else reset();
        return false;
      }
      len = b;
      got = 0;
      st = (len == 0) ? JST_CKHI : JST_DATA;
      return false;
    case JST_DATA:
      data[got++] = b;
      if (got >= len) st = JST_CKHI;
      return false;
    case JST_CKHI:
      ckhi = b;
      st = JST_CKLO;
      return false;
    case JST_CKLO: {
      // Verify: cover = JST_REG + LEN_HI(0x00 req? no—len is single byte on wire
      // for requests... requests carry LEN as ONE byte) — cover is exactly
      // the bytes between byte1 and checksum: reg, len, data.
      uint32_t s = (uint32_t)reg + (uint32_t)len;
      for (uint8_t i = 0; i < len; i++) s += data[i];
      uint16_t want = (uint16_t)(0x10000UL - (s & 0xFFFFUL));
      uint16_t got_ck = ((uint16_t)ckhi << 8) | b;
      ck_ok = (want == got_ck);
      st = JST_END;
      return false;
    }
    case JST_END:
      if (b == 0x77 && ck_ok) {
        out.is_write = is_write;
        out.reg = reg;
        out.len = len;
        for (uint8_t i = 0; i < len; i++) out.data[i] = data[i];
        reset();
        return true;
      }
      if (b == 0xDD) restart();
      else reset();
      return false;
  }
  reset();
  return false;
}

const uint8_t *reply_for(uint8_t reg, bool is_write, size_t &out_len) {
  if (is_write) return 0;  // option A: never answer writes
  switch (reg) {
    case 0x03: out_len = BMS_RESPONSE_LEN; return BMS_RESPONSE;
    case 0x04: out_len = BMS_RESPONSE_CELLS_LEN; return BMS_RESPONSE_CELLS;
    case 0x05: out_len = BMS_RESPONSE_NAME_LEN; return BMS_RESPONSE_NAME;
    default: return 0;     // option A: silence on unknown registers
  }
}

void PollTracker::note_poll(unsigned long now) {
  if (seen) {
    unsigned long dt = now - last;
    if (dt > 0) {
      ema = have_ema ? (ema * 3 + dt + 2) / 4 : dt;
      have_ema = true;
    }
  } else {
    seen = true;
  }
  last = now;
}

unsigned long PollTracker::threshold_ms() const {
  if (!have_ema) return 2000UL;
  unsigned long t = ema * 2 + 500;
  if (t < 2000UL) t = 2000UL;
  if (t > 10000UL) t = 10000UL;
  return t;
}
