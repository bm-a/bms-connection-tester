// Host stub: ESPmDNS (test_web ONLY). Records begin/end so tests prove the
// .local advertisement follows the WiFi kill switch. Never in firmware.
#pragma once

inline bool g_mdns_up = false;

struct MDNSClass {
  bool begin(const char *) {
    g_mdns_up = true;
    return true;
  }
  void end() { g_mdns_up = false; }
};
static MDNSClass MDNS;
