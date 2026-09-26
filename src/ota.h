#pragma once
#include <stdint.h>
#include <stddef.h>

// v2.3 OTA decision logic — hardware-independent (no Arduino dependency) so
// it compiles on the host for Unity tests, exactly like bms_protocol.*.
// All network I/O (GitHub API query, HTTPUpdate download) lives in main.cpp
// (ESP-only, never host-built); this file only decides WHAT to fetch and WHEN.

// Release asset names (see GitHub Releases: plain + n16r8 triples, plus the
// Waveshare 8DI8RO board line).
#define OTA_ASSET_8MB "firmware.bin"
#define OTA_ASSET_N16R8 "n16r8-firmware.bin"
#define OTA_ASSET_WAVESHARE "waveshare-firmware.bin"
// Build variant tag: s3-n16r8 sets -DFW_IS_N16R8=1 (numeric -D only; quoted
// -D strings do not survive the build-flag pipeline). The Waveshare
// 8DI8RO build sets -DBOARD_WAVESHARE_8DI8RO=1. Everything else is 8mb.
#if defined(BOARD_WAVESHARE_8DI8RO)
#define FW_VARIANT "waveshare"
#elif defined(FW_IS_N16R8)
#define FW_VARIANT "n16r8"
#else
#define FW_VARIANT "8mb"
#endif

// Compare dotted versions, tolerating a leading 'v' ("v2.3" == "2.3").
// Returns -1 / 0 / +1 (a<b, a==b, a>b). Non-numeric tails compare as 0;
// empty/unparseable input is treated as 0.0 (never newer than a release).
// Board-line suffix: "2.7-ws2" > "2.7-ws1" (numeric); a suffixed version
// and a plain one never outrank each other (different product lines —
// cross-line auto-update is meaningless and unsafe), so "2.7" == "2.7-ws1".
int ota_cmp_version(const char *a, const char *b);

// True when tag is a Waveshare board-line release tag: v<dots>-ws<digits>
// (e.g. "v2.7-ws1"). Used by the Waveshare build to scan /releases for its
// own prerelease line (which never becomes /releases/latest).
bool ota_tag_is_waveshare_line(const char *tag);

// Asset file to download for a variant tag ("8mb", "n16r8", "waveshare").
const char *ota_asset_for_variant(const char *variant);

// Download URL for a release tag + variant, written into out (out_len bytes).
// Returns true on success. Tag keeps its 'v' ("v2.3"), asset has no prefix.
bool ota_download_url(const char *tag, const char *variant, char *out,
                      size_t out_len);

// Auto-check gate: true when a check may start now. now/last_check are
// millis(); interval_ms is the configured cadence (0 = manual only).
// Suppressed while a relay sequence runs, while the bus was recently active
// (bus_silent_ms < min_bus_silent_ms), or when STA has no internet.
bool ota_should_check(unsigned long now, unsigned long last_check,
                      unsigned long interval_ms, bool auto_enabled,
                      bool sta_online, bool seq_running,
                      unsigned long bus_silent_ms,
                      unsigned long min_bus_silent_ms);

// v2.3 runtime OTA state (owned by main.cpp, surfaced on the dashboard).
struct OtaState {
  bool auto_enabled = false;
  unsigned long interval_ms = 24UL * 3600UL * 1000UL;  // daily
  unsigned long last_check_ms = 0;
  char latest_tag[16] = "";    // e.g. "v2.4", empty = unknown yet
  bool update_pending = false;  // latest_tag newer than FW_VERSION
  char status[48] = "never checked";
};
