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
  std::string bytes;
  bool begin(size_t = UPDATE_SIZE_UNKNOWN) {
    begun = true;
    finished = false;
    error = false;
    bytes.clear();
    return true;
  }
  size_t write(const uint8_t *data, size_t len) {
    if (!begun || !data) {
      error = true;
      return 0;
    }
    bytes.append((const char *)data, len);
    return len;
  }
  bool end(bool = true) {
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
    bytes.clear();
  }
};
inline UpdateStub Update;
