// Host execution of the REAL web_ui.cpp (v2.0 dashboard) against stubbed
// Arduino/WiFi/WebServer/Preferences (./Arduino.h etc.). Proves login,
// session, validation/clamping, NVS round-trip, API handlers and fuzz
// robustness without any ESP32. g++-only (see run_tests.sh); NOT in pio.
#include <unity.h>
#include <map>
#include <string>
#include "Arduino.h"
#include "WebServer.h"
#include "WiFi.h"
#include "Preferences.h"
#include "Update.h"
#include "ESPmDNS.h"
#include "esp_system.h"
#include "../../src/relay_ctrl.h"
#include "../../src/ota.h"
#include "../../src/web_ui.h"

void setUp(void) {}
void tearDown(void) {}

static Bms2Config cfg;
static RelaySequencer seq;
static SpoofPlan spoof;
static OtaState ota;
static bool link_green = false;
static uint8_t spoofFrameA[SPOOF_FRAME_LEN];
static uint8_t spoofFrameB[SPOOF_FRAME_LEN];
static void on_cfg() {
  build_spoof_frame(cfg, 1, spoofFrameA);
  build_spoof_frame(cfg, 2, spoofFrameB);
}
static bool ota_check_called = false;
static void on_ota() { ota_check_called = true; }
static bool ota_install_called = false;
static void on_ota_install() { ota_install_called = true; }
static WebCtx ctx;

// v2.3.1: no login wall. Sensitive endpoints (/api/admin, /api/ota,
// /update) take the admin password inline per request.
static std::string with_pass(const std::string &body) {
  if (body.empty() || body == "{}") return "{\"pass\":\"admin123\"}";
  return body.substr(0, body.size() - 1) + ",\"pass\":\"admin123\"}";
}

static WebServer::Resp admin_post(const std::string &path,
                                  const std::string &body) {
  return WebServer::post(path, with_pass(body));
}

static void nvs_clear() {
  Preferences p;
  p.begin("bms2", false);
  p.clear();
  p.end();
}

static void fresh_env() {
  nvs_clear();
  Preferences::nvs_commits_reset();
  cfg = Bms2Config();
  seq.begin(&cfg);
  spoof.cancel();
  ota = OtaState();
  ota_check_called = false;
  ota_install_called = false;
  link_green = false;
  g_mock_millis = 1000000UL;
  g_restart_requested = false;
  g_wifi_status = WL_DISCONNECTED;
  g_wifi_begun = false;
  Update.reset();
  ctx.cfg = &cfg;
  ctx.seq = &seq;
  ctx.spoof = &spoof;
  ctx.link_green = &link_green;
  ctx.on_config_changed = on_cfg;
  ctx.ota = &ota;
  ctx.on_ota_check = on_ota;
  ctx.on_ota_install = on_ota_install;
  web_setup(ctx);
  Preferences::nvs_commits_reset();  // web_setup tracks boot (its own write)
}

static bool has(const std::string &s, const std::string &sub) {
  return s.find(sub) != std::string::npos;
}

void test_root_open_dashboard(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "id=relays"));
  TEST_ASSERT_TRUE(has(r.body, "FIRE now") || has(r.body, "FIRE"));
  // Unknown paths still land on the dashboard (captive portal), not a 404.
  r = WebServer::get("/generate_204");
  TEST_ASSERT_EQUAL_INT(302, r.code);
  TEST_ASSERT_TRUE(r.headers["Location"] == "/");
}

void test_state_public_no_secrets(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"relays\":[0,0,0,0,0,0,0,0]"));
  // Password values are NEVER in the public state (no login wall anymore).
  TEST_ASSERT_FALSE(has(r.body, "ap_pass"));
  TEST_ASSERT_FALSE(has(r.body, "sta_pass"));
  TEST_ASSERT_FALSE(has(r.body, "a_user"));
  TEST_ASSERT_FALSE(has(r.body, "bms12345"));
}

void test_state_defaults(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"relays\":[0,0,0,0,0,0,0,0]"));
  TEST_ASSERT_TRUE(has(r.body, "\"step\":250"));
  TEST_ASSERT_TRUE(has(r.body, "\"hseq\":30000"));
  TEST_ASSERT_TRUE(has(r.body, "\"swp\":3"));
  TEST_ASSERT_TRUE(has(r.body, "\"hall\":300000"));
  TEST_ASSERT_TRUE(has(r.body, "\"nrel\":8"));
  TEST_ASSERT_TRUE(has(r.body, "\"rmode\":0"));
  TEST_ASSERT_TRUE(has(r.body, "\"ssec\":5"));
  TEST_ASSERT_TRUE(has(r.body, "\"s2sec\":10"));
  TEST_ASSERT_TRUE(has(r.body, "\"fw\":\"2.4\""));
  TEST_ASSERT_TRUE(has(r.body, "\"link\":false"));
}

void test_relay_override(void) {
  fresh_env();
  WebServer::Resp r =
      WebServer::post("/api/relay", "{\"i\":3,\"on\":1}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(seq.relayOn(3));
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"relays\":[0,0,0,1,0,0,0,0]"));
  r = WebServer::post("/api/relay", "{\"i\":3,\"on\":0}");
  TEST_ASSERT_FALSE(seq.relayOn(3));
  r = WebServer::post("/api/relay", "{\"i\":99,\"on\":1}");
  TEST_ASSERT_EQUAL_INT(400, r.code);  // out of range rejected
}

void test_seq_start_stop(void) {
  fresh_env();
  WebServer::Resp r =
      WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(seq.running());
  r = WebServer::post("/api/seq", "{\"cmd\":\"stop\"}");
  TEST_ASSERT_FALSE(seq.running());
  TEST_ASSERT_EQUAL_INT(0, seq.onCount());
}

void test_config_validation_persist(void) {
  fresh_env();
  WebServer::Resp r = WebServer::post(
      "/api/config",
      "{\"rmode\":2,\"nrel\":4,\"step\":5,\"hseq\":99999999,\"swp\":2,"
      "\"hall\":60000,\"bmode\":9,\"alow\":0}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_INT(RELAY_CHASE, cfg.relay_mode);
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
  TEST_ASSERT_EQUAL_INT(100, cfg.step_delay_ms);     // floored (R4)
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.hold_seq_ms);  // capped (ms now)
  TEST_ASSERT_EQUAL_UINT8(2, cfg.chase_sweeps);
  TEST_ASSERT_EQUAL_UINT32(60000, cfg.hold_all_ms);
  TEST_ASSERT_EQUAL_INT(BTN_HOLD_ABORT, cfg.button_mode);  // bad -> default
  TEST_ASSERT_FALSE(cfg.active_low);
  // Persistence: flush the deferred save, trash RAM copy, reload from NVS.
  web_tick(g_mock_millis + 2000);
  cfg.step_delay_ms = 12345;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(100, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.hold_seq_ms);
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
}

void test_spoof_fire_cancel(void) {
  fresh_env();
  WebServer::Resp r = WebServer::post(
      "/api/spoof",
      "{\"cmd\":\"fire\",\"sv\":1000,\"sa\":1000,\"sc\":1000,\"ssoc\":100,"
      "\"ssec\":5,\"s2v\":888,\"s2a\":888,\"s2c\":888,\"s2soc\":188,"
      "\"s2sec\":10,\"spin\":25}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(spoof.active(g_mock_millis));
  TEST_ASSERT_EQUAL_UINT8(1, spoof.stage(g_mock_millis));
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"spoof\":true"));
  TEST_ASSERT_TRUE(has(r.body, "\"stage\":1"));
  // Advance into stage 2 (100-first, then 88.8/188).
  g_mock_millis += 5000;
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"stage\":2"));
  r = WebServer::post("/api/spoof", "{\"cmd\":\"cancel\"}");
  TEST_ASSERT_FALSE(spoof.active(g_mock_millis));
}

void test_admin_gate_and_validation(void) {
  fresh_env();
  // No password -> refused, nothing applied.
  WebServer::Resp r = WebServer::post("/api/admin", "{\"ap_ssid\":\"X\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_TRUE(has(r.body, "admin password required"));
  // Wrong password -> refused too.
  r = WebServer::post("/api/admin",
                      "{\"ap_ssid\":\"X\",\"pass\":\"nope\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  // Short AP password rejected even with the right admin pass.
  r = admin_post("/api/admin", "{\"ap_pass\":\"short\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));  // 8+ chars enforced
  r = admin_post("/api/admin",
                 "{\"ap_ssid\":\"Bench\",\"ap_pass\":\"longpass1\","
                 "\"ap_ch\":9,\"a_pass\":\"s3cret\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  // New admin pass works from here on, old one doesn't (RAM + NVS reload).
  r = WebServer::post("/api/admin",
                      "{\"cmd\":\"reboot\",\"pass\":\"admin123\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);
  r = admin_post("/api/admin", "{\"cmd\":\"reboot\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));  // pass helper still old -> no
  r = WebServer::post("/api/admin", "{\"cmd\":\"reboot\",\"pass\":\"s3cret\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(g_restart_requested);
  r = WebServer::get("/api/state");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"ap_ch\":9"));
  TEST_ASSERT_FALSE(has(r.body, "longpass1"));  // secrets never leak
}

void test_ota_gate(void) {
  fresh_env();
  // Toggling OTA without the password changes nothing.
  WebServer::Resp r = WebServer::post("/api/ota", "{\"ota_auto\":1}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_FALSE(ota.auto_enabled);
  r = admin_post("/api/ota", "{\"ota_auto\":1}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(ota.auto_enabled);
  // Check-now also gated (it can flash firmware via the uplink).
  g_wifi_status = WL_CONNECTED;
  ota.auto_enabled = true;
  r = WebServer::post("/api/ota", "{\"cmd\":\"check\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_FALSE(ota_check_called);
}

void test_fuzz_posts(void) {
  fresh_env();
  const char *eps[] = {"/api/relay", "/api/seq", "/api/config", "/api/spoof",
                       "/api/admin", "/api/ota"};
  const char *bad[] = {"{{{", "", "{\"i\":-1,\"on\":9999999999999999999}",
                       "{\"step\":\"abc\",\"hold\":null}", "not json at all",
                       "{\"cmd\":\"__proto__\",\"ssoc\":300,\"ssec\":0}"};
  for (auto ep : eps) {
    for (auto b : bad) {
      WebServer::Resp r = WebServer::post(ep, b);
      TEST_ASSERT_TRUE(r.code == 200 || r.code == 400);
      // Same garbage WITH the admin password: still no crash, no apply.
      r = WebServer::post(ep, with_pass(b));
      TEST_ASSERT_TRUE(r.code == 200 || r.code == 400);
    }
  }
  // Config still sane after garbage.
  TEST_ASSERT_TRUE(cfg.step_delay_ms >= 100 && cfg.step_delay_ms <= 60000);
  TEST_ASSERT_TRUE(cfg.hold_seq_ms <= 3600000);
  TEST_ASSERT_TRUE(cfg.chase_sweeps <= 100);
  TEST_ASSERT_TRUE(cfg.hold_all_ms <= 3600000);
  TEST_ASSERT_TRUE(cfg.spoof_seconds >= 1 && cfg.spoof_seconds <= 120);
  TEST_ASSERT_TRUE(cfg.s2_seconds >= 1 && cfg.s2_seconds <= 120);
  TEST_ASSERT_TRUE(cfg.relay_count >= 1 && cfg.relay_count <= 8);
}

void test_factory_reset_clears(void) {
  fresh_env();
  WebServer::post("/api/config", "{\"step\":4321}");
  TEST_ASSERT_EQUAL_INT(4321, cfg.step_delay_ms);
  WebServer::Resp r = admin_post("/api/admin", "{\"cmd\":\"reset\"}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(g_restart_requested);
  cfg = Bms2Config();  // reboot wipes RAM; NVS (now empty) yields defaults
  web_setup(ctx);      // reboot reloads -> factory defaults
  TEST_ASSERT_EQUAL_INT(250, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_INT(5, cfg.spoof_seconds);
  TEST_ASSERT_EQUAL_INT(10, cfg.s2_seconds);
}

void test_portal_redirect_flow(void) {
  fresh_env();
  // Captive-portal probes (any unknown URL) land on "/" ...
  WebServer::Resp r = WebServer::get("/generate_204");
  TEST_ASSERT_EQUAL_INT(302, r.code);
  TEST_ASSERT_TRUE(r.headers["Location"] == "/");
  // ... which serves the open dashboard (no login wall since v2.3.1).
  r = WebServer::get("/");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "id=relays"));
}

void test_nvs_migration_v2(void) {
  // A released-v2 NVS image: version tag 2, hold in seconds, single stage.
  nvs_clear();
  Preferences p;
  p.begin("bms2", false);
  p.putUChar("v", 2);
  p.putUShort("hold", 30);
  p.putUShort("step", 700);
  p.putUShort("sv", 600);
  p.putUShort("sa", 601);
  p.putUShort("sc", 602);
  p.putUChar("ssoc", 50);
  p.putUShort("ssec", 7);
  p.end();
  cfg = Bms2Config();
  seq.begin(&cfg);
  web_setup(ctx);  // migrates the v2 image in place (no nvs_clear)
  TEST_ASSERT_EQUAL_UINT32(30000, cfg.hold_seq_ms);  // seconds -> ms
  TEST_ASSERT_EQUAL_UINT8(3, cfg.chase_sweeps);  // chase keeps auto-sweeps
  TEST_ASSERT_EQUAL_UINT32(30000, cfg.hold_all_ms);
  TEST_ASSERT_EQUAL_INT(700, cfg.step_delay_ms);  // untouched keys kept
  TEST_ASSERT_EQUAL_INT(600, cfg.s2_v_tenth);     // customs -> stage 2
  TEST_ASSERT_EQUAL_INT(601, cfg.s2_a_tenth);
  TEST_ASSERT_EQUAL_INT(50, cfg.s2_soc);
  TEST_ASSERT_EQUAL_INT(7, cfg.s2_seconds);
  TEST_ASSERT_EQUAL_INT(1000, cfg.spoof_v_tenth);  // stage 1 = 100 defaults
  TEST_ASSERT_EQUAL_INT(100, cfg.spoof_soc);
  TEST_ASSERT_EQUAL_INT(5, cfg.spoof_seconds);
  // Next save stamps v3 with the ms hold (flush the deferred write first).
  WebServer::post("/api/config", "{\"step\":701}");
  web_tick(g_mock_millis + 2000);
  Preferences q;
  q.begin("bms2", true);
  TEST_ASSERT_EQUAL_INT(3, q.getUChar("v", 0));
  TEST_ASSERT_EQUAL_UINT32(30000, q.getUInt("hseq", 0));
  q.end();
}

void test_spoof_pin_clamp_and_persist(void) {
  fresh_env();
  // Safe pin sticks; reserved pins fall back to 21.
  WebServer::Resp r = WebServer::post("/api/spoof",
                                      "{\"cmd\":\"fire\",\"spin\":44}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_UINT8(44, cfg.spoof_pin);
  r = WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"spin\":16}");
  TEST_ASSERT_EQUAL_UINT8(21, cfg.spoof_pin);  // UART RX is reserved
  r = WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"spin\":18}");
  TEST_ASSERT_EQUAL_UINT8(21, cfg.spoof_pin);  // WiFi kill pin is reserved
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"spin\":21"));
  // Persists across reload (FIRE saves like all spoof values).
  web_tick(g_mock_millis + 2000);
  WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"spin\":44}");
  web_tick(g_mock_millis + 2000);
  cfg.spoof_pin = 21;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_UINT8(44, cfg.spoof_pin);
}

void test_ota_install_gate(void) {
  fresh_env();
  // Nothing pending -> refused even with the password.
  WebServer::Resp r = admin_post("/api/ota", "{\"cmd\":\"install\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_FALSE(ota_install_called);
  // Pending but STA offline -> refused, no install attempt.
  ota.update_pending = true;
  strncpy(ota.latest_tag, "v9.9", sizeof(ota.latest_tag));
  r = admin_post("/api/ota", "{\"cmd\":\"install\"}");
  TEST_ASSERT_TRUE(has(r.body, "STA offline"));
  TEST_ASSERT_FALSE(ota_install_called);
  // Pending + online + password -> dispatches to main.cpp.
  g_wifi_status = WL_CONNECTED;
  ota.auto_enabled = true;
  admin_post("/api/admin",
             "{\"sta_en\":1,\"sta_ssid\":\"Hot\",\"sta_pass\":\"pw123456\"}");
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(2, web_sta_state());
  r = admin_post("/api/ota", "{\"cmd\":\"install\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(ota_install_called);
  // No password -> refused.
  ota_install_called = false;
  r = WebServer::post("/api/ota", "{\"cmd\":\"install\"}");
  TEST_ASSERT_TRUE(has(r.body, "admin password required"));
  TEST_ASSERT_FALSE(ota_install_called);
}

void test_wifi_kill_switch(void) {
  fresh_env();
  web_wifi_set(false);  // grounded pin: everything down
  TEST_ASSERT_EQUAL_INT(WIFI_OFF, g_wifi_mode);
  web_wifi_set(false);  // idempotent: no crash, still off
  TEST_ASSERT_EQUAL_INT(WIFI_OFF, g_wifi_mode);
  web_wifi_set(true);  // released: AP back with the saved identity
  TEST_ASSERT_EQUAL_INT(WIFI_AP, g_wifi_mode);
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_EQUAL_INT(200, r.code);
}

void test_relay_count_api(void) {
  fresh_env();
  WebServer::post("/api/config", "{\"nrel\":4}");
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
  WebServer::Resp r =
      WebServer::post("/api/relay", "{\"i\":5,\"on\":1}");
  TEST_ASSERT_EQUAL_INT(400, r.code);  // beyond count: OFF zone
  r = WebServer::post("/api/relay", "{\"i\":3,\"on\":1}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(seq.relayOn(3));
  WebServer::post("/api/config", "{\"nrel\":99}");
  TEST_ASSERT_EQUAL_INT(8, cfg.relay_count);  // clamped
  WebServer::post("/api/config", "{\"nrel\":0}");
  TEST_ASSERT_EQUAL_INT(1, cfg.relay_count);
}

void test_chase_runs_single(void) {
  fresh_env();
  WebServer::post("/api/config", "{\"rmode\":2,\"nrel\":8}");
  TEST_ASSERT_EQUAL_INT(RELAY_CHASE, cfg.relay_mode);
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  seq.tick(g_mock_millis);
  TEST_ASSERT_TRUE(seq.running());
  TEST_ASSERT_EQUAL_UINT8(1, seq.onCount());  // chase: exactly one lit
}

void test_ota_api(void) {
  fresh_env();
  WebServer::Resp r = admin_post("/api/ota", "{\"ota_auto\":1}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(ota.auto_enabled);
  // Check-now without the STA uplink is refused (office has no internet).
  r = admin_post("/api/ota", "{\"cmd\":\"check\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_FALSE(ota_check_called);
  // Bring STA online: check-now dispatches to main.cpp.
  admin_post("/api/admin",
             "{\"sta_en\":1,\"sta_ssid\":\"Hot\",\"sta_pass\":\"pw123456\"}");
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);  // simulated reboot picks up the STA creds
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(2, web_sta_state());
  r = admin_post("/api/ota", "{\"cmd\":\"check\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(ota_check_called);
  // OTA preference persists across the (simulated) reboot.
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);
  TEST_ASSERT_TRUE(ota.auto_enabled);
}

void test_update_upload(void) {
  fresh_env();
  // Valid 8 MB image head (0xE9 magic, flash code 3) + random body.
  std::string fw(1500, '\0');
  for (size_t i = 0; i < fw.size(); i++) fw[i] = (char)(i * 31 + 7);
  fw[0] = (char)0xE9;
  fw[3] = (char)0x30;
  std::map<std::string, std::string> upargs;
  upargs["pass"] = "admin123";
  WebServer::Resp r = WebServer::upload("/update", "firmware.bin", fw,
                                        std::map<std::string, std::string>(),
                                        upargs);
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "UPDATE OK"));
  TEST_ASSERT_TRUE(Update.finished);
  TEST_ASSERT_EQUAL_INT(1, Update.end_calls);  // single-end rule (v2.4)
  TEST_ASSERT_TRUE(Update.begin_size != 0xFFFFFFFFu);  // explicit budget
  TEST_ASSERT_EQUAL_INT((int)fw.size(), (int)Update.bytes.size());
  TEST_ASSERT_TRUE(Update.bytes == fw);
  TEST_ASSERT_TRUE(g_restart_requested);
  // Uploads without the admin password are refused: nothing flashes.
  fresh_env();
  r = WebServer::upload("/update", "firmware.bin", fw);
  TEST_ASSERT_EQUAL_INT(403, r.code);
  TEST_ASSERT_TRUE(has(r.body, "admin password required"));
  TEST_ASSERT_FALSE(Update.finished);
}

void test_update_rejects(void) {
  fresh_env();
  std::string fw(1500, '\0');
  for (size_t i = 0; i < fw.size(); i++) fw[i] = (char)(i * 31 + 7);
  fw[0] = (char)0xE9;
  fw[3] = (char)0x30;
  std::map<std::string, std::string> upargs;
  upargs["pass"] = "admin123";
  // Wrong variant file: the 8 MB box refuses the N16R8 asset by NAME.
  WebServer::Resp r = WebServer::upload("/update", "n16r8-firmware.bin", fw,
                                        std::map<std::string, std::string>(),
                                        upargs);
  TEST_ASSERT_EQUAL_INT(400, r.code);
  TEST_ASSERT_TRUE(has(r.body, "wrong file"));
  TEST_ASSERT_FALSE(Update.finished);
  // Garbage bytes (ELF, config JSON): bad magic, nothing committed.
  std::string elf(1500, 'x');
  elf[0] = 0x7F;
  r = WebServer::upload("/update", "firmware.bin", elf,
                        std::map<std::string, std::string>(), upargs);
  TEST_ASSERT_EQUAL_INT(400, r.code);
  TEST_ASSERT_TRUE(has(r.body, "magic"));
  TEST_ASSERT_FALSE(Update.finished);
  // 16 MB image on an 8 MB flash chip: rejected on the head bytes.
  std::string big16 = fw;
  big16[3] = (char)0x40;
  r = WebServer::upload("/update", "firmware.bin", big16,
                        std::map<std::string, std::string>(), upargs);
  TEST_ASSERT_EQUAL_INT(400, r.code);
  TEST_ASSERT_TRUE(has(r.body, "more flash"));
  TEST_ASSERT_FALSE(Update.finished);
  // Tiny sketch budget: explicit TOO_BIG instead of dying mid-stream.
  // (Page granularity is 4 KB, so this case needs a file bigger than the
  // 0x1000 budget a 0x2000-free chip reports.)
  std::string big(5000, 'q');
  big[0] = (char)0xE9;
  big[3] = (char)0x30;
  g_esp_free_sketch = 0x2000;
  r = WebServer::upload("/update", "firmware.bin", big,
                        std::map<std::string, std::string>(), upargs);
  TEST_ASSERT_EQUAL_INT(400, r.code);
  TEST_ASSERT_TRUE(has(r.body, "too big"));
  TEST_ASSERT_FALSE(Update.finished);
  g_esp_free_sketch = 0x800000;
}

void test_update_pass_order_independent(void) {
  // The pass field may stream AFTER the file part (some stacks reorder
  // multipart parts); the verdict is judged once, in the done handler.
  fresh_env();
  std::string fw(1500, '\0');
  for (size_t i = 0; i < fw.size(); i++) fw[i] = (char)(i * 31 + 7);
  fw[0] = (char)0xE9;
  fw[3] = (char)0x30;
  std::map<std::string, std::string> upargs;
  upargs["pass"] = "admin123";
  WebServer::Resp r = WebServer::upload("/update", "firmware.bin", fw,
                                        std::map<std::string, std::string>(),
                                        upargs, false /* pass streams last */);
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(Update.finished);
  TEST_ASSERT_TRUE(Update.bytes == fw);
}

void test_sta_uplink(void) {
  fresh_env();
  TEST_ASSERT_EQUAL_INT(0, web_sta_state());  // AP-only by default
  TEST_ASSERT_FALSE(g_wifi_begun);
  admin_post("/api/admin",
             "{\"sta_en\":1,\"sta_ssid\":\"Hot\",\"sta_pass\":\"pw\"}");
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(1, web_sta_state());  // connecting, non-blocking
  TEST_ASSERT_TRUE(g_wifi_begun);
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(2, web_sta_state());
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"sta\":2"));
  // An attempt that never links falls back to AP-only after 30 s.
  fresh_env();
  admin_post("/api/admin", "{\"sta_en\":1,\"sta_ssid\":\"Hot\"}");
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  g_mock_millis += 31000;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(0, web_sta_state());
}

void test_save_coalescing(void) {
  fresh_env();
  // Three rapid saves (like the dashboard's double-POST buttons): the bench
  // reacts instantly from RAM, but flash is untouched until the idle flush.
  WebServer::post("/api/config", "{\"step\":1000}");
  TEST_ASSERT_EQUAL_INT(1000, cfg.step_delay_ms);  // RAM: instant
  WebServer::post("/api/config", "{\"step\":2000}");
  WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"ssec\":6}");
  TEST_ASSERT_EQUAL_INT(0, (int)Preferences::nvs_commits());
  web_tick(g_mock_millis + 500);  // too soon: still coalescing
  TEST_ASSERT_EQUAL_INT(0, (int)Preferences::nvs_commits());
  web_tick(g_mock_millis + 2000);  // idle flush: exactly ONE flash commit
  TEST_ASSERT_EQUAL_INT(1, (int)Preferences::nvs_commits());
  // And the coalesced values are what got committed.
  cfg.step_delay_ms = 0;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(2000, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_INT(6, cfg.spoof_seconds);
}

void test_reboot_flushes_pending_save(void) {
  fresh_env();
  WebServer::post("/api/config", "{\"step\":4321}");
  TEST_ASSERT_EQUAL_INT(0, (int)Preferences::nvs_commits());
  // Reboot commits synchronously first: nothing lost, no tick needed.
  admin_post("/api/admin", "{\"cmd\":\"reboot\"}");
  TEST_ASSERT_TRUE(g_restart_requested);
  TEST_ASSERT_EQUAL_INT(1, (int)Preferences::nvs_commits());
  cfg.step_delay_ms = 0;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(4321, cfg.step_delay_ms);
}

void test_industrial_config(void) {
  fresh_env();
  WebServer::Resp r = WebServer::post(
      "/api/config",
      "{\"loop\":1,\"cpause\":99999999,\"clim\":50,\"stag\":99999,\"dir\":5}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(cfg.loop_enabled);
  TEST_ASSERT_EQUAL_UINT32(60000, cfg.cycle_pause_ms);  // capped (R6)
  TEST_ASSERT_EQUAL_INT(50, cfg.cycle_limit);
  TEST_ASSERT_EQUAL_INT(1000, cfg.allon_stagger_ms);    // capped (R5)
  TEST_ASSERT_EQUAL_INT(0, cfg.seq_dir);                  // bad -> forward
  WebServer::post("/api/config", "{\"dir\":1}");
  TEST_ASSERT_EQUAL_INT(1, cfg.seq_dir);
  admin_post("/api/admin", "{\"auto\":1}");
  TEST_ASSERT_TRUE(cfg.boot_autostart);
  web_tick(g_mock_millis + 2000);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"loop\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"clim\":50"));
  TEST_ASSERT_TRUE(has(r.body, "\"dir\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"auto\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"lbl0\":\"R1\""));  // default tile names
  // Persists across reload.
  cfg.loop_enabled = false;
  web_setup(ctx);
  TEST_ASSERT_TRUE(cfg.loop_enabled);
  TEST_ASSERT_EQUAL_INT(50, cfg.cycle_limit);
}

void test_relay_labels(void) {
  fresh_env();
  WebServer::Resp r = WebServer::post(
      "/api/config", "{\"lbl0\":\"HORN\",\"lbl1\":\"LIGHT BAR\"}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  TEST_ASSERT_EQUAL_STRING("LIGHT BAR", cfg.relay_label[1]);
  web_tick(g_mock_millis + 2000);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"lbl0\":\"HORN\""));
  // Hostile labels rejected: quotes (JSON/HTML break), backslash, too long.
  WebServer::post("/api/config", "{\"lbl0\":\"A\\\"B\"}");
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  WebServer::post("/api/config", "{\"lbl0\":\"AAAAAAAAAAAAAAAA\"}");
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  WebServer::post("/api/config", "{\"lbl0\":\"A\\\\B\"}");
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
}

void test_counters_state(void) {
  fresh_env();
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  seq.tick(g_mock_millis + 3500);  // 8 sequential actuations done
  seq.tick(g_mock_millis + 3500 + 30000);  // hold_seq default -> cycle done
  TEST_ASSERT_EQUAL_UINT(1, seq.cyclesDone());
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"cycles\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"acts\":8"));
}

void test_spoof_save_only(void) {
  fresh_env();
  // Save stages values + pin WITHOUT firing (v2.4): no spoof window opens.
  WebServer::Resp r = WebServer::post(
      "/api/spoof", "{\"cmd\":\"save\",\"sv\":520,\"ssec\":9,\"spin\":2}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_EQUAL_INT(520, cfg.spoof_v_tenth);
  TEST_ASSERT_EQUAL_INT(9, cfg.spoof_seconds);
  TEST_ASSERT_EQUAL_INT(2, cfg.spoof_pin);
  TEST_ASSERT_EQUAL_UINT8(0, spoof.stage(g_mock_millis));
  // FIRE still saves AND triggers.
  r = WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"sv\":530}");
  TEST_ASSERT_EQUAL_INT(530, cfg.spoof_v_tenth);
  TEST_ASSERT_EQUAL_UINT8(1, spoof.stage(g_mock_millis));
}

void test_config_loop_hold_reject(void) {
  fresh_env();
  // R9: loop + hold 0 can never complete a cycle — the save is refused
  // and the bench keeps its previous config.
  WebServer::Resp r = WebServer::post(
      "/api/config", "{\"loop\":1,\"hseq\":0,\"hall\":0}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_TRUE(has(r.body, "finite hold"));
  TEST_ASSERT_FALSE(cfg.loop_enabled);
  // Finite hold + loop: accepted.
  r = WebServer::post("/api/config", "{\"loop\":1,\"hseq\":5000}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(cfg.loop_enabled);
}

void test_config_mode_switch_needs_idle(void) {
  fresh_env();
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(seq.running());
  // R10: mode switch mid-run is refused; the running mode is untouched.
  WebServer::Resp r = WebServer::post("/api/config", "{\"rmode\":2}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_TRUE(has(r.body, "stop the sequence"));
  TEST_ASSERT_EQUAL_INT(RELAY_SEQUENTIAL, cfg.relay_mode);
  WebServer::post("/api/seq", "{\"cmd\":\"stop\"}");
  g_mock_millis += 1000;  // past the R12 dead-band
  r = WebServer::post("/api/config", "{\"rmode\":2}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_EQUAL_INT(RELAY_CHASE, cfg.relay_mode);
}

void test_config_pause_floor(void) {
  fresh_env();
  // R26: pause clamps to the 500 ms floor (R6), never honored literally.
  WebServer::post("/api/config", "{\"cpause\":0}");
  TEST_ASSERT_EQUAL_UINT32(500, cfg.cycle_pause_ms);
  WebServer::post("/api/config", "{\"cpause\":99999999}");
  TEST_ASSERT_EQUAL_UINT32(60000, cfg.cycle_pause_ms);
}

void test_seq_deadband_api(void) {
  fresh_env();
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  WebServer::post("/api/seq", "{\"cmd\":\"stop\"}");
  // R12: immediate restart refused with a named error.
  WebServer::Resp r = WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_TRUE(has(r.body, "settling"));
  TEST_ASSERT_FALSE(seq.running());
  g_mock_millis += 600;
  r = WebServer::post("/api/seq", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(seq.running());
}

void test_sta_test_flow(void) {
  fresh_env();
  // No SSID anywhere: named error, no radio touched.
  WebServer::Resp r = admin_post("/api/sta", "{\"cmd\":\"test\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  // Supplied credentials: test starts, AP stays up.
  r = admin_post("/api/sta",
                 "{\"cmd\":\"test\",\"ssid\":\"Hot\",\"sta_pass\":\"pw\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(g_wifi_begun);
  // Link comes up: ok + RSSI/IP recorded, radio back to AP-only.
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis + 1000);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"sta_test\":2"));
  TEST_ASSERT_TRUE(has(r.body, "RSSI"));
  TEST_ASSERT_TRUE(has(r.body, "192.168.43.12"));
  TEST_ASSERT_FALSE(g_wifi_begun);
  // Never links: 30 s deadline -> named failure, AP-only again.
  fresh_env();
  r = admin_post("/api/sta",
                 "{\"cmd\":\"test\",\"ssid\":\"Hot\",\"sta_pass\":\"pw\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  g_mock_millis += 31000;
  web_tick(g_mock_millis);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"sta_test\":3"));
  TEST_ASSERT_FALSE(g_wifi_begun);
}

void test_console_verbs(void) {
  fresh_env();
  // Free verbs.
  WebServer::Resp r = WebServer::post("/api/cmd", "{\"cmd\":\"HELP\"}");
  TEST_ASSERT_TRUE(has(r.body, "START STOP"));
  r = WebServer::post("/api/cmd", "{\"cmd\":\"status\"}");
  TEST_ASSERT_TRUE(has(r.body, "LINK RED"));
  r = WebServer::post("/api/cmd", "{\"cmd\":\"version\"}");
  TEST_ASSERT_TRUE(has(r.body, "2.4"));
  r = WebServer::post("/api/cmd", "{\"cmd\":\"bogus\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  // Privileged verbs need the password...
  r = WebServer::post("/api/cmd", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(has(r.body, "admin password required"));
  TEST_ASSERT_FALSE(seq.running());
  // ...and then they work.
  r = admin_post("/api/cmd", "{\"cmd\":\"start\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"out\":\"started\""));
  TEST_ASSERT_TRUE(seq.running());
  r = admin_post("/api/cmd", "{\"cmd\":\"stop\"}");
  TEST_ASSERT_FALSE(seq.running());
  g_mock_millis += 1000;
  r = admin_post("/api/cmd", "{\"cmd\":\"fire\"}");
  TEST_ASSERT_EQUAL_UINT8(1, spoof.stage(g_mock_millis));
}

void test_backup_restore(void) {
  fresh_env();
  WebServer::post("/api/config",
                  "{\"rmode\":1,\"nrel\":4,\"step\":1000,\"stag\":60}");
  WebServer::post("/api/spoof", "{\"cmd\":\"save\",\"sv\":520}");
  // Backup exports everything but secrets.
  WebServer::Resp r = WebServer::get("/api/backup");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"backup\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"nrel\":4"));
  TEST_ASSERT_TRUE(has(r.body, "\"sv\":520"));
  TEST_ASSERT_FALSE(has(r.body, "bms12345"));  // AP secret never leaves
  TEST_ASSERT_FALSE(has(r.body, "admin123"));  // admin secret never leaves
  // Restore onto a wiped box brings settings back (not passwords).
  std::string bk = r.body;
  fresh_env();
  TEST_ASSERT_EQUAL_INT(8, cfg.relay_count);
  size_t pp = bk.rfind('}');
  std::string rb = bk.substr(0, pp) + ",\"pass\":\"admin123\"}";
  r = WebServer::post("/api/restore", rb);
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(has(r.body, "passwords not restored"));
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
  TEST_ASSERT_EQUAL_INT(520, cfg.spoof_v_tenth);
  TEST_ASSERT_EQUAL_INT(RELAY_ALL_ON, cfg.relay_mode);
  // Non-backup JSON is refused.
  r = admin_post("/api/restore", "{\"nrel\":2}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_TRUE(has(r.body, "not a bms-tester backup"));
}

void test_reset_keepwifi(void) {
  fresh_env();
  admin_post("/api/admin", "{\"ap_ssid\":\"Bench-AP\"}");
  WebServer::post("/api/config", "{\"step\":4321}");
  web_tick(g_mock_millis + 2000);
  // Reset-4: settings wiped, identity kept, reboot follows.
  WebServer::Resp r =
      admin_post("/api/admin", "{\"cmd\":\"reset_keepwifi\"}");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(g_restart_requested);
  cfg = Bms2Config();  // simulated reboot: RAM wiped, NVS reloaded
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(250, cfg.step_delay_ms);  // settings: defaults
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"ap_ssid\":\"Bench-AP\""));  // kept
}

void test_bootcount_info(void) {
  fresh_env();  // boot #1 (NVS was cleared)
  WebServer::Resp r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"boot\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"reset\":\"power-on\""));
  web_setup(ctx);  // simulated reboot -> boot #2
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"boot\":2"));
  // Reset-99 zeroes the counter without rebooting.
  r = admin_post("/api/admin", "{\"cmd\":\"bootcount_reset\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"boot\":0"));
  // Reset reason follows the (stubbed) hardware cause.
  g_esp_reset_reason_code = ESP_RST_PANIC;
  web_setup(ctx);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "\"reset\":\"panic\""));
  g_esp_reset_reason_code = ESP_RST_POWERON;
}

void test_info_fields(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/api/state");
  // Tasmota Status 1/2/4/5 surface, bench edition.
  for (const char *k : {"\"variant\":\"8mb\"", "\"flash_kb\":8192",
                        "\"sketch_free\":", "\"heap\":", "\"psram\":",
                        "\"uptime_s\":", "\"rssi\":", "\"sta_ip\":",
                        "\"sta_mac\":", "\"ota_url\":", "\"ota_int_h\":"}) {
    TEST_ASSERT_TRUE(has(r.body, k));
  }
}

void test_ota_url_flow(void) {
  fresh_env();
  // Garbage URL refused with a named error.
  WebServer::Resp r = admin_post("/api/ota", "{\"ota_url\":\"ftp://x/y.txt\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  // Good URL + cadence persist.
  r = admin_post("/api/ota",
                 "{\"ota_url\":\"http://192.168.1.9:8000/n16r8-firmware.bin\","
                 "\"ota_int_h\":6}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  r = WebServer::get("/api/state");
  TEST_ASSERT_TRUE(has(r.body, "192.168.1.9"));
  TEST_ASSERT_TRUE(has(r.body, "\"ota_int_h\":6"));
  // URL upgrade needs STA online first (enable + reboot + link)...
  admin_post("/api/admin", "{\"sta_en\":1,\"sta_ssid\":\"Hot\"}");
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  r = admin_post("/api/ota", "{\"cmd\":\"url_upgrade\"}");
  TEST_ASSERT_TRUE(has(r.body, "STA offline"));
  TEST_ASSERT_FALSE(ota_install_called);
  // ...then it fires the shared install path.
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  r = admin_post("/api/ota", "{\"cmd\":\"url_upgrade\"}");
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(ota_install_called);
}

void test_mdns_follows_wifi(void) {
  fresh_env();
  TEST_ASSERT_TRUE(g_mdns_up);  // advertised with the AP
  web_wifi_set(false);          // kill switch drops everything
  TEST_ASSERT_FALSE(g_mdns_up);
  web_wifi_set(true);
  TEST_ASSERT_TRUE(g_mdns_up);  // release brings it back
}

void run_all() {
  RUN_TEST(test_root_open_dashboard);
  RUN_TEST(test_state_public_no_secrets);
  RUN_TEST(test_portal_redirect_flow);
  RUN_TEST(test_state_defaults);
  RUN_TEST(test_relay_override);
  RUN_TEST(test_seq_start_stop);
  RUN_TEST(test_config_validation_persist);
  RUN_TEST(test_spoof_fire_cancel);
  RUN_TEST(test_spoof_pin_clamp_and_persist);
  RUN_TEST(test_admin_gate_and_validation);
  RUN_TEST(test_ota_gate);
  RUN_TEST(test_ota_install_gate);
  RUN_TEST(test_wifi_kill_switch);
  RUN_TEST(test_fuzz_posts);
  RUN_TEST(test_factory_reset_clears);
  RUN_TEST(test_nvs_migration_v2);
  RUN_TEST(test_relay_count_api);
  RUN_TEST(test_chase_runs_single);
  RUN_TEST(test_ota_api);
  RUN_TEST(test_update_upload);
  RUN_TEST(test_update_rejects);
  RUN_TEST(test_update_pass_order_independent);
  RUN_TEST(test_spoof_save_only);
  RUN_TEST(test_config_loop_hold_reject);
  RUN_TEST(test_config_mode_switch_needs_idle);
  RUN_TEST(test_config_pause_floor);
  RUN_TEST(test_seq_deadband_api);
  RUN_TEST(test_sta_test_flow);
  RUN_TEST(test_console_verbs);
  RUN_TEST(test_backup_restore);
  RUN_TEST(test_reset_keepwifi);
  RUN_TEST(test_bootcount_info);
  RUN_TEST(test_info_fields);
  RUN_TEST(test_ota_url_flow);
  RUN_TEST(test_mdns_follows_wifi);
  RUN_TEST(test_sta_uplink);
  RUN_TEST(test_save_coalescing);
  RUN_TEST(test_reboot_flushes_pending_save);
  RUN_TEST(test_industrial_config);
  RUN_TEST(test_relay_labels);
  RUN_TEST(test_counters_state);
}

int main() {
  UNITY_BEGIN();
  run_all();
  return UNITY_END();
}
