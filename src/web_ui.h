#pragma once
#include "relay_ctrl.h"
#include "ota.h"

// v2.4 web UI: always-on WiFi AP + relay/spoof/admin pages + NVS.
// v2.3.1: no login wall (WPA2 is the gate); reboot/reset/upload/OTA-admin
// carry the admin password per request.
// v2.4: Tasmota-grade update path, per-mode relay menu, console, config
// backup/restore, custom OTA URL, STA uplink test, info card, mDNS.
// ESP-ONLY (needs Arduino WiFi/WebServer/Preferences). Never compiled on host.
// The v1.x base loop stays untouched — main.cpp just calls web_setup() once
// and web_tick() every loop; all handlers are short and non-blocking so the
// 9600-baud RS485 path keeps real-time priority.

#ifdef ARDUINO

// Shared context owned by main.cpp.
struct WebCtx {
  Bms2Config *cfg = nullptr;
  RelaySequencer *seq = nullptr;
  SpoofPlan *spoof = nullptr;    // v2.3: two-stage plan (was SpoofWindow)
  const bool *link_green = nullptr;  // live LED state for the dashboard
  void (*on_config_changed)() = nullptr;  // rebuild spoof frames + apply
  OtaState *ota = nullptr;        // v2.3: OTA status surfaced on dashboard
  void (*on_ota_check)() = nullptr;  // v2.3: main.cpp performs a check now
  void (*on_ota_install)() = nullptr;  // v2.3.1: dashboard Install button
};

// AP defaults (overridden by NVS once saved).
#define WEB_AP_SSID_DEFAULT "BMS-Tester"
#define WEB_AP_PASS_DEFAULT "bms12345"
#define WEB_AP_CHANNEL_DEFAULT 6

// NVS namespace + version tag.
#define WEB_NVS_NS "bms2"
#define WEB_NVS_VERSION 3

void web_setup(WebCtx &ctx);
void web_tick(unsigned long now);
// Long-press fallback (button held 10 s): wipe NVS + reboot. Called by main.
// Long-press fallback (button held 10 s): wipe NVS + reboot. Called by main.
void web_factory_reset();
// v2.4: the ONE reboot path (flush NVS, grace beat, restart). All reboot/
// reset/update-success flows funnel through here.
void web_reboot_now();
// v2.4: boot counter reset (Tasmota Reset-99; no reboot needed).
void web_bootcount_reset();
// v2.4: custom firmware URL (Tasmota OtaUrl; "" = GitHub releases default).
const char *web_ota_url();
// v2.3.1 WiFi kill switch (main.cpp polls the pin): false = AP+portal+
// server fully down now; true = back up (default on at boot).
void web_wifi_set(bool on);
// v2.3: STA uplink state for main.cpp's OTA gate. 0 = off, 1 = connecting,
// 2 = online (has address; internet assumed when online).
int web_sta_state();
// v2.6 daily meter counting (R39-R40): software-only link-gap heuristic,
// fed from web_tick() every loop. No button, no extra GPIO.
// v2.6 manual "new day" reset (no RTC; boot persists).
void web_meter_reset();

#endif  // ARDUINO
