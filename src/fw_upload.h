#pragma once
#include <stdint.h>
#include <stddef.h>

// v2.4 Tasmota-grade firmware-update gates — hardware-independent (no Arduino
// dependency) so they compile on the host for Unity tests, exactly like
// bms_protocol.* / relay_ctrl.h / ota.h. Both update paths (manual /update
// upload in web_ui.cpp, OTA-pull install in main.cpp) enforce these before
// committing a single byte to flash. Inspired by Tasmota's HandleUploadLoop:
// explicit max-sketch size, 0xE9 magic + flash-size-vs-chip check, exact
// variant asset match, single Update.end(), one error vocabulary.

// Update decision outcome. The dashboard and the /update page surface
// fw_err_str() verbatim so every failure names its cause (never a bare 403).
enum FwUploadErr : uint8_t {
  FW_OK = 0,
  FW_BAD_PASS,      // admin password missing/wrong
  FW_TOO_BIG,       // image larger than the free sketch space
  FW_WRONG_FILE,    // filename is not this variant's release asset
  FW_BAD_MAGIC,     // first bytes are not an ESP boot image (want 0xE9)
  FW_FLASH_TOO_BIG,  // image built for more flash than this chip has
  FW_WRITE_FAIL,    // Update.write/begin/end failed mid-stream
  FW_BEGIN_FAIL,    // Update.begin rejected (partition/state)
};

inline const char *fw_err_str(FwUploadErr e) {
  switch (e) {
    case FW_OK: return "ok";
    case FW_BAD_PASS: return "admin password required";
    case FW_TOO_BIG: return "file too big for free sketch space";
    case FW_WRONG_FILE: return "wrong file for this board (need the matching release asset)";
    case FW_BAD_MAGIC: return "not a firmware image (bad magic)";
    case FW_FLASH_TOO_BIG: return "image needs more flash than this chip";
    case FW_WRITE_FAIL: return "flash write failed";
    case FW_BEGIN_FAIL: return "update begin failed";
    default: return "update failed";
  }
}

// Tasmota's explicit size: (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000.
// Never UPDATE_SIZE_UNKNOWN: an explicit budget fails fast with TOO_BIG
// instead of dying mid-stream. Returns 0 when nothing is usable.
inline uint32_t fw_max_sketch_space(uint32_t free_sketch_bytes) {
  if (free_sketch_bytes <= 0x1000u) return 0;
  return (free_sketch_bytes - 0x1000u) & 0xFFFFF000u;
}

// Declared content length (0 = unknown/chunked) fits the budget.
inline bool fw_size_ok(uint32_t decl_len, uint32_t max_space) {
  if (decl_len == 0) return true;  // unknown: stream, fail on write error
  return decl_len != 0 && decl_len <= max_space && max_space != 0;
}

// Release asset this variant must receive ("8mb" or "n16r8").
inline const char *fw_asset_for_variant(const char *variant) {
  if (variant) {
    const char *v = variant;
    if ((v[0] == 'n' || v[0] == 'N') && v[1] == '1' && v[2] == '6') {
      static const char a[] = "n16r8-firmware.bin";
      return a;
    }
  }
  static const char b[] = "firmware.bin";
  return b;
}

// Uploaded filename must be this variant's exact release asset name.
// ("firmware.bin" is a suffix of "n16r8-firmware.bin", so suffix matching
// would cross-flash — compare the EXACT basename instead. Browsers may send
// a fake path like C:\fakepath\firmware.bin, hence the basename strip.)
inline bool fw_filename_ok(const char *filename, const char *variant) {
  if (!filename || !*filename) return false;
  const char *base = filename;
  for (const char *p = filename; *p; p++) {
    if (*p == '/' || *p == '\\') base = p + 1;
  }
  const char *asset = fw_asset_for_variant(variant);
  size_t i = 0;
  for (;;) {
    char x = base[i], y = asset[i];
    if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
    if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
    if (x != y) return false;
    if (x == '\0') return true;
    i++;
  }
}

// ESP image header flash-size nibble (byte 3, high half) -> bytes.
// Codes: 0=1MB 1=2MB 2=4MB 3=8MB 4=16MB 5=32MB 6=64MB 7=128MB.
inline uint32_t fw_flash_code_bytes(uint8_t code) {
  if (code > 7) return 0;
  return 0x100000u << code;  // 1 MB << code
}

// First-chunk gate on the streamed image head (needs >= 4 bytes):
// 0xE9 magic + image flash size <= the real chip. Returns FW_OK,
// FW_BAD_MAGIC, or FW_FLASH_TOO_BIG.
inline FwUploadErr fw_gate_image_head(const uint8_t *data, size_t len,
                                      uint32_t chip_flash_bytes) {
  if (!data || len < 4) return FW_BAD_MAGIC;
  if (data[0] != 0xE9) return FW_BAD_MAGIC;
  uint32_t need = fw_flash_code_bytes((uint8_t)((data[3] & 0xF0u) >> 4));
  if (need == 0 || need > chip_flash_bytes) return FW_FLASH_TOO_BIG;
  return FW_OK;
}
