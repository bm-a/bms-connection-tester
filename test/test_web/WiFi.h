// Host stub: ESP32 WiFi (test_web ONLY). AP calls succeed silently.
#pragma once
#include "DNSServer.h"

#define WIFI_AP 1

struct WiFiClass {
  void mode(int) {}
  void softAPConfig(const IPAddress &, const IPAddress &, const IPAddress &) {}
  bool softAP(const char *, const char *, int) { return true; }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
};
static WiFiClass WiFi;
