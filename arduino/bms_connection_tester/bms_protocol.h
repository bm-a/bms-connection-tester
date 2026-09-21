#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#endif

#define FW_VERSION "2.4"

// JBD / Xiaoxiang Smart BMS, 9600 8N1, half-duplex over RS485.
// v2.x base (frozen): answers Basic Info (0x03), Cell Voltages (0x04) and
// Device Name (0x05); stays SILENT on writes/unknown registers (option A).
// Green lamp = any well-formed meter frame seen within the adaptive window.
// v2.0 adds (additive only): 8-relay sequencer, always-on AP web UI,
// spoof window — see relay_ctrl.h / web_ui.h.

// Fixed meter request for register 0x03: DD A5 03 00 FF FD 77
extern const uint8_t BMS_REQUEST[7];
#define BMS_REQUEST_LEN 7

// Canned SOC 100% response (golden, byte-exact real capture).
extern const uint8_t BMS_RESPONSE[34];
#define BMS_RESPONSE_LEN 34

// Canned 14S cell voltages (14 x 3714 mV = 52.0 V, matches golden frame).
extern const uint8_t BMS_RESPONSE_CELLS[35];
#define BMS_RESPONSE_CELLS_LEN 35

// Canned device name ("TEST-14S100A").
extern const uint8_t BMS_RESPONSE_NAME[19];
#define BMS_RESPONSE_NAME_LEN 19

// Generic JBD checksum: 0x10000 - sum(buf[0..len-1]).
// Caller chooses the slice. Requests: CMD+LEN(+DATA).
// Responses: STATUS/LEN_HI+LEN_LO+DATA (echoed command byte excluded).
uint16_t jbd_checksum(const uint8_t *buf, size_t len);

// Strict 7-byte request matcher (v1.0 compat, kept for tests).
bool matches_request(const uint8_t *buf);

// Rolling live status, fixed 2 s window (v1.0 compat, kept for tests).
inline bool connection_active(unsigned long now_ms, unsigned long last_valid_ms) {
  return (now_ms - last_valid_ms) < 2000UL;
}

// ---- streaming JBD request parser (since v1.1) ----
#define JBD_MAX_DATA 64

struct JbdFrame {
  bool is_write;              // true when byte1 == 0x5A
  uint8_t reg;                // requested register
  uint8_t len;                // payload length (0 for reads)
  uint8_t data[JBD_MAX_DATA]; // payload (reads carry none)
};

class JbdParser {
 public:
  JbdParser() { reset(); }
  void reset();
  // Feed one byte. Returns true exactly once per VALID frame
  // (checksum-verified, exact length, 0x77 terminator).
  bool feed(uint8_t b, JbdFrame &out);
 private:
  enum State { JST_HUNT, JST_B1, JST_REG, JST_LEN, JST_DATA, JST_CKHI, JST_CKLO, JST_END };
  State st;
  bool is_write;
  bool ck_ok;
  uint8_t reg, len, ckhi;
  uint8_t data[JBD_MAX_DATA];
  uint8_t got;
  void restart();
};

// ---- reply dispatcher (option A: silence unless known read) ----
const uint8_t *reply_for(uint8_t reg, bool is_write, size_t &out_len);

// ---- adaptive green window (since v1.1) ----
struct PollTracker {
  unsigned long last = 0;
  bool seen = false;
  unsigned long ema = 0;      // EMA of poll interval, ms
  bool have_ema = false;
  void note_poll(unsigned long now);
  unsigned long threshold_ms() const;  // 2000 boot .. clamp(2*ema+500, 2s, 10s)
  bool active(unsigned long now) const {
    return seen && (now - last) < threshold_ms();
  }
};
