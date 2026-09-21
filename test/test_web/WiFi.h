// Host stub: ESP32 WiFi (test_web ONLY). AP calls succeed silently.
#pragma once

#define WIFI_AP 1

struct WiFiClass {
  void mode(int) {}
  bool softAP(const char *, const char *, int) { return true; }
};
static WiFiClass WiFi;
