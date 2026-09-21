// Host stub: ESP32 WiFi (test_web ONLY). AP calls succeed silently; STA
// link state is test-driven via g_wifi_status (0 = idle, 3 = connected).
#pragma once
#include "DNSServer.h"

#define WIFI_AP 1
#define WIFI_AP_STA 3
#define WL_DISCONNECTED 0
#define WL_CONNECTED 3

inline int g_wifi_status = WL_DISCONNECTED;
inline bool g_wifi_begun = false;

struct WiFiClass {
  void mode(int) {}
  void softAPConfig(const IPAddress &, const IPAddress &, const IPAddress &) {}
  bool softAP(const char *, const char *, int) { return true; }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  void begin(const char *, const char *) { g_wifi_begun = true; }
  int status() { return g_wifi_status; }
  void disconnect(bool = false) { g_wifi_begun = false; }
};
static WiFiClass WiFi;
