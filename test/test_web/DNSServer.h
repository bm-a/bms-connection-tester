// Host stub: IPAddress + DNSServer (test_web ONLY). DNS calls no-op.
#pragma once
#include "Arduino.h"

class IPAddress {
 public:
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    b_[0] = a;
    b_[1] = b;
    b_[2] = c;
    b_[3] = d;
  }
  std::string str() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b_[0], b_[1], b_[2], b_[3]);
    return buf;
  }

 private:
  uint8_t b_[4];
};

class DNSServer {
 public:
  int startCalls = 0;
  int processCalls = 0;
  bool start(uint16_t, const char *, const IPAddress &) {
    startCalls++;
    return true;
  }
  void processNextRequest() { processCalls++; }
};
