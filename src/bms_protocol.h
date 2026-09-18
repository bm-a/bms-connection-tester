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

// JBD / Xiaoxiang Smart BMS, 9600 8N1, half-duplex over RS485.
// Meter polls with a fixed 7-byte read of register 0x03 (Basic Info).
// Tester impersonates the battery with ONE canned SOC-100% reply.

// Fixed meter request: DD A5 03 00 FF FD 77
extern const uint8_t BMS_REQUEST[7];
#define BMS_REQUEST_LEN 7

// Canned SOC 100% response (52.00V, 0A, 100Ah/100Ah, RSOC=100%,
// 14S, 2x temp @ 25.0C). Sent VERBATIM — never recomputed at runtime.
// Checksum FC DA covers STATUS+LEN+DATA (packet[2:-3]), i.e. it
// EXCLUDES the echoed command byte (see bms_protocol.cpp note).
extern const uint8_t BMS_RESPONSE[34];
#define BMS_RESPONSE_LEN 34

// Generic JBD checksum: 0x10000 - sum(buf[0..len-1]), big-endian.
// Caller chooses the slice (request: CMD+LEN; response: STATUS+LEN+DATA).
uint16_t jbd_checksum(const uint8_t *buf, size_t len);

// Strict 7-byte request matcher. No substring/loose matching.
bool matches_request(const uint8_t *buf);

// Rolling live status: green while a valid poll arrived <2000ms ago.
// Unsigned subtraction => millis() rollover safe. Boots red (lastValid=0).
inline bool connection_active(unsigned long now_ms, unsigned long last_valid_ms) {
  return (now_ms - last_valid_ms) < 2000UL;
}
