// Host stub: Arduino-ESP32 Update (test_web ONLY). Records the firmware
// bytes the /update upload handler writes so tests prove the plumbing
// (begin -> write* -> end -> reboot) without touching real flash.
#pragma once
#include "Arduino.h"
#include <string>

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFFu

struct UpdateStub {
  bool begun = false;
  bool finished = false;
  bool error = false;
  int end_calls = 0;  // v2.4: proves the single-end rule (Tasmota Done-only)
  size_t begin_size = 0;
  bool fail_begin = false;  // v2.4: inject BEGIN_FAIL
  size_t fail_write_at = 0;  // v2.4: writes at/after this offset fail (0 = ok)
  std::string bytes;
  bool begin(size_t s = UPDATE_SIZE_UNKNOWN) {
    begun = true;
    finished = false;
    error = false;
    begin_size = s;
    bytes.clear();
    if (fail_begin) {
      begun = false;
      error = true;
      return false;
    }
    return true;
  }
  // NOTE: real Arduino Update takes uint8_t* (non-const) — mirrored here
  // on purpose so host builds catch const-drift before firmware does.
  size_t write(uint8_t *data, size_t len) {
    if (!begun || !data) {
      error = true;
      return 0;
    }
    if (fail_write_at > 0 && bytes.size() + len > fail_write_at) {
      error = true;
      return 0;
    }
    bytes.append((const char *)data, len);
    return len;
  }
  // end(false) = Tasmota-style abort: staged bytes never boot, and the
  // stub mirrors it (finished stays false, no error flagged).
  bool end(bool finalize = true) {
    end_calls++;
    if (!finalize) {
      begun = false;
      return true;
    }
    if (!begun || bytes.empty()) {
      error = true;
      return false;
    }
    finished = true;
    begun = false;
    return true;
  }
  bool hasError() { return error; }
  void reset() {
    begun = false;
    finished = false;
    error = false;
    end_calls = 0;
    begin_size = 0;
    fail_begin = false;
    fail_write_at = 0;
    bytes.clear();
  }
};
inline UpdateStub Update;
