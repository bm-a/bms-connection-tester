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
static WebCtx ctx;
static std::string cookie;

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
  web_setup(ctx);
}

static std::string login(const char *u, const char *p, bool remember = false) {
  std::map<std::string, std::string> args;
  args["u"] = u;
  args["p"] = p;
  if (remember) args["remember"] = "1";
  WebServer::Resp r = WebServer::request(
      "POST", "/login", std::map<std::string, std::string>(), "", args);
  TEST_ASSERT_EQUAL_INT(302, r.code);
  auto it = r.headers.find("Set-Cookie");
  TEST_ASSERT_TRUE(it != r.headers.end());
  std::string sc = it->second;  // "BMS2=<tok>; Path=/; ..."
  auto pos = sc.find("BMS2=");
  TEST_ASSERT_TRUE(pos != std::string::npos);
  auto end = sc.find(';', pos);
  return sc.substr(pos, end == std::string::npos ? end : end - pos);
}

static std::map<std::string, std::string> auth() {
  if (cookie.empty()) cookie = login("admin", "admin123");
  std::map<std::string, std::string> h;
  h["Cookie"] = cookie;
  return h;
}

static bool has(const std::string &s, const std::string &sub) {
  return s.find(sub) != std::string::npos;
}

void test_login_page_public(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/login");
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "action=/login"));
}

void test_root_requires_auth(void) {
  fresh_env();
  WebServer::Resp r = WebServer::get("/");
  TEST_ASSERT_EQUAL_INT(302, r.code);
  TEST_ASSERT_TRUE(r.headers["Location"] == "/login");
  r = WebServer::get("/api/state");
  TEST_ASSERT_EQUAL_INT(401, r.code);
}

void test_login_bad(void) {
  fresh_env();
  std::map<std::string, std::string> args;
  args["u"] = "admin";
  args["p"] = "wrong";
  WebServer::Resp r = WebServer::request(
      "POST", "/login", std::map<std::string, std::string>(), "", args);
  TEST_ASSERT_EQUAL_INT(401, r.code);
}

void test_login_good_dashboard(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::get("/", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "id=relays"));
  TEST_ASSERT_TRUE(has(r.body, "FIRE now") || has(r.body, "FIRE"));
}

void test_state_defaults(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::get("/api/state", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"relays\":[0,0,0,0,0,0,0,0]"));
  TEST_ASSERT_TRUE(has(r.body, "\"step\":500"));
  TEST_ASSERT_TRUE(has(r.body, "\"hseq\":30000"));
  TEST_ASSERT_TRUE(has(r.body, "\"hch\":30000"));
  TEST_ASSERT_TRUE(has(r.body, "\"hall\":300000"));
  TEST_ASSERT_TRUE(has(r.body, "\"nrel\":8"));
  TEST_ASSERT_TRUE(has(r.body, "\"rmode\":0"));
  TEST_ASSERT_TRUE(has(r.body, "\"ssec\":5"));
  TEST_ASSERT_TRUE(has(r.body, "\"s2sec\":10"));
  TEST_ASSERT_TRUE(has(r.body, "\"fw\":\"2.3\""));
  TEST_ASSERT_TRUE(has(r.body, "\"link\":false"));
}

void test_relay_override(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r =
      WebServer::post("/api/relay", "{\"i\":3,\"on\":1}", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(seq.relayOn(3));
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"relays\":[0,0,0,1,0,0,0,0]"));
  r = WebServer::post("/api/relay", "{\"i\":3,\"on\":0}", auth());
  TEST_ASSERT_FALSE(seq.relayOn(3));
  r = WebServer::post("/api/relay", "{\"i\":99,\"on\":1}", auth());
  TEST_ASSERT_EQUAL_INT(400, r.code);  // out of range rejected
}

void test_seq_start_stop(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r =
      WebServer::post("/api/seq", "{\"cmd\":\"start\"}", auth());
  TEST_ASSERT_TRUE(seq.running());
  r = WebServer::post("/api/seq", "{\"cmd\":\"stop\"}", auth());
  TEST_ASSERT_FALSE(seq.running());
  TEST_ASSERT_EQUAL_INT(0, seq.onCount());
}

void test_config_validation_persist(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/config",
      "{\"rmode\":2,\"nrel\":4,\"step\":5,\"hseq\":99999999,\"hch\":5000,"
      "\"hall\":60000,\"bmode\":9,\"alow\":0}",
      auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_INT(RELAY_CHASE, cfg.relay_mode);
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
  TEST_ASSERT_EQUAL_INT(50, cfg.step_delay_ms);      // floored
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.hold_seq_ms);  // capped (ms now)
  TEST_ASSERT_EQUAL_UINT32(5000, cfg.hold_chase_ms);
  TEST_ASSERT_EQUAL_UINT32(60000, cfg.hold_all_ms);
  TEST_ASSERT_EQUAL_INT(BTN_HOLD_ABORT, cfg.button_mode);  // bad -> default
  TEST_ASSERT_FALSE(cfg.active_low);
  // Persistence: flush the deferred save, trash RAM copy, reload from NVS.
  web_tick(g_mock_millis + 2000);
  cfg.step_delay_ms = 12345;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(50, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.hold_seq_ms);
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
}

void test_spoof_fire_cancel(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/spoof",
      "{\"cmd\":\"fire\",\"sv\":1000,\"sa\":1000,\"sc\":1000,\"ssoc\":100,"
      "\"ssec\":5,\"s2v\":888,\"s2a\":888,\"s2c\":888,\"s2soc\":188,"
      "\"s2sec\":10}",
      auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(spoof.active(g_mock_millis));
  TEST_ASSERT_EQUAL_UINT8(1, spoof.stage(g_mock_millis));
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"spoof\":true"));
  TEST_ASSERT_TRUE(has(r.body, "\"stage\":1"));
  // Advance into stage 2 (100-first, then 88.8/188).
  g_mock_millis += 5000;
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"stage\":2"));
  r = WebServer::post("/api/spoof", "{\"cmd\":\"cancel\"}", auth());
  TEST_ASSERT_FALSE(spoof.active(g_mock_millis));
}

void test_admin_validation(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/admin", "{\"ap_pass\":\"short\"}", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));  // 8+ chars enforced
  r = WebServer::post("/api/admin",
                      "{\"ap_ssid\":\"Bench\",\"ap_pass\":\"longpass1\","
                      "\"ap_ch\":9,\"a_user\":\"tech\",\"a_pass\":\"s3cret\"}",
                      auth());
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  // New creds work, old ones don't (persistence across reload too).
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);
  std::map<std::string, std::string> args;
  args["u"] = "admin";
  args["p"] = "admin123";
  r = WebServer::request("POST", "/login",
                         std::map<std::string, std::string>(), "", args);
  TEST_ASSERT_EQUAL_INT(401, r.code);
  cookie = login("tech", "s3cret");
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "\"ap_ch\":9"));
}

void test_session_expiry_relogin(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  g_mock_millis += 31UL * 60UL * 1000UL;  // past 30 min sliding window
  WebServer::Resp r = WebServer::get("/api/state", auth());
  TEST_ASSERT_EQUAL_INT(401, r.code);
  cookie = login("admin", "admin123");  // re-login works
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
}

void test_logout(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::get("/logout", auth());
  TEST_ASSERT_EQUAL_INT(302, r.code);
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_EQUAL_INT(401, r.code);
}

void test_fuzz_posts(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  const char *eps[] = {"/api/relay", "/api/seq", "/api/config", "/api/spoof",
                       "/api/admin", "/api/ota"};
  const char *bad[] = {"{{{", "", "{\"i\":-1,\"on\":9999999999999999999}",
                       "{\"step\":\"abc\",\"hold\":null}", "not json at all",
                       "{\"cmd\":\"__proto__\",\"ssoc\":300,\"ssec\":0}"};
  for (auto ep : eps) {
    for (auto b : bad) {
      WebServer::Resp r = WebServer::post(ep, b, auth());
      TEST_ASSERT_TRUE(r.code == 200 || r.code == 400 || r.code == 401);
    }
  }
  // Config still sane after garbage.
  TEST_ASSERT_TRUE(cfg.step_delay_ms >= 50 && cfg.step_delay_ms <= 60000);
  TEST_ASSERT_TRUE(cfg.hold_seq_ms <= 3600000);
  TEST_ASSERT_TRUE(cfg.hold_chase_ms <= 3600000);
  TEST_ASSERT_TRUE(cfg.hold_all_ms <= 3600000);
  TEST_ASSERT_TRUE(cfg.spoof_seconds >= 1 && cfg.spoof_seconds <= 120);
  TEST_ASSERT_TRUE(cfg.s2_seconds >= 1 && cfg.s2_seconds <= 120);
  TEST_ASSERT_TRUE(cfg.relay_count >= 1 && cfg.relay_count <= 8);
}

void test_factory_reset_clears(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::post("/api/config", "{\"step\":4321}", auth());
  TEST_ASSERT_EQUAL_INT(4321, cfg.step_delay_ms);
  WebServer::Resp r =
      WebServer::post("/api/admin", "{\"cmd\":\"reset\"}", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(g_restart_requested);
  cfg = Bms2Config();  // reboot wipes RAM; NVS (now empty) yields defaults
  web_setup(ctx);      // reboot reloads -> factory defaults
  TEST_ASSERT_EQUAL_INT(500, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_INT(5, cfg.spoof_seconds);
  TEST_ASSERT_EQUAL_INT(10, cfg.s2_seconds);
}

void test_portal_redirect_flow(void) {
  fresh_env();
  // Captive-portal probes (any unknown URL) land on "/" ...
  WebServer::Resp r = WebServer::get("/generate_204");
  TEST_ASSERT_EQUAL_INT(302, r.code);
  TEST_ASSERT_TRUE(r.headers["Location"] == "/");
  // ... which itself sends unauthed browsers to the login page.
  r = WebServer::get("/");
  TEST_ASSERT_EQUAL_INT(302, r.code);
  TEST_ASSERT_TRUE(r.headers["Location"] == "/login");
  // Authed users hitting unknown paths land on the dashboard, not a 404.
  cookie = login("admin", "admin123");
  r = WebServer::get("/generate_204", auth());
  TEST_ASSERT_EQUAL_INT(302, r.code);
  r = WebServer::get("/", auth());
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
  TEST_ASSERT_EQUAL_UINT32(30000, cfg.hold_seq_ms);  // seconds -> ms, all modes
  TEST_ASSERT_EQUAL_UINT32(30000, cfg.hold_chase_ms);
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
  cookie = login("admin", "admin123");
  WebServer::post("/api/config", "{\"step\":701}", auth());
  web_tick(g_mock_millis + 2000);
  Preferences q;
  q.begin("bms2", true);
  TEST_ASSERT_EQUAL_INT(3, q.getUChar("v", 0));
  TEST_ASSERT_EQUAL_UINT32(30000, q.getUInt("hseq", 0));
  q.end();
}

void test_remember_login_persists(void) {
  fresh_env();
  std::string cA = login("admin", "admin123", true);
  std::map<std::string, std::string> hA;
  hA["Cookie"] = cA;
  // Simulate reboot: RAM session lost (a plain login overwrites it).
  cookie = login("admin", "admin123");
  // The remembered cookie still works via its NVS slot.
  WebServer::Resp r = WebServer::get("/api/state", hA);
  TEST_ASSERT_EQUAL_INT(200, r.code);
}

void test_remember_expiry(void) {
  fresh_env();
  std::string cA = login("admin", "admin123", true);
  std::map<std::string, std::string> hA;
  hA["Cookie"] = cA;
  g_mock_millis += 31UL * 24UL * 3600UL * 1000UL;  // past the 30-day window
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::get("/api/state", hA);
  TEST_ASSERT_EQUAL_INT(401, r.code);
}

void test_token_eviction(void) {
  fresh_env();
  std::string c[5];
  for (int i = 0; i < 5; i++) c[i] = login("admin", "admin123", true);
  cookie = login("admin", "admin123");  // RAM holds the plain session now
  std::map<std::string, std::string> h;
  h["Cookie"] = c[0];
  WebServer::Resp r = WebServer::get("/api/state", h);
  TEST_ASSERT_EQUAL_INT(401, r.code);  // oldest slot evicted by the 5th login
  h["Cookie"] = c[4];
  r = WebServer::get("/api/state", h);
  TEST_ASSERT_EQUAL_INT(200, r.code);
}

void test_relay_count_api(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::post("/api/config", "{\"nrel\":4}", auth());
  TEST_ASSERT_EQUAL_INT(4, cfg.relay_count);
  WebServer::Resp r =
      WebServer::post("/api/relay", "{\"i\":5,\"on\":1}", auth());
  TEST_ASSERT_EQUAL_INT(400, r.code);  // beyond count: OFF zone
  r = WebServer::post("/api/relay", "{\"i\":3,\"on\":1}", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(seq.relayOn(3));
  WebServer::post("/api/config", "{\"nrel\":99}", auth());
  TEST_ASSERT_EQUAL_INT(8, cfg.relay_count);  // clamped
  WebServer::post("/api/config", "{\"nrel\":0}", auth());
  TEST_ASSERT_EQUAL_INT(1, cfg.relay_count);
}

void test_chase_runs_single(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::post("/api/config", "{\"rmode\":2,\"nrel\":8}", auth());
  TEST_ASSERT_EQUAL_INT(RELAY_CHASE, cfg.relay_mode);
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}", auth());
  seq.tick(g_mock_millis);
  TEST_ASSERT_TRUE(seq.running());
  TEST_ASSERT_EQUAL_UINT8(1, seq.onCount());  // chase: exactly one lit
}

void test_ota_api(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post("/api/ota", "{\"ota_auto\":1}", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(ota.auto_enabled);
  // Check-now without the STA uplink is refused (office has no internet).
  r = WebServer::post("/api/ota", "{\"cmd\":\"check\"}", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":0"));
  TEST_ASSERT_FALSE(ota_check_called);
  // Bring STA online: check-now dispatches to main.cpp.
  WebServer::post("/api/admin",
                  "{\"sta_en\":1,\"sta_ssid\":\"Hot\",\"sta_pass\":\"pw123456\"}",
                  auth());
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);  // simulated reboot picks up the STA creds
  cookie = login("admin", "admin123");
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(2, web_sta_state());
  r = WebServer::post("/api/ota", "{\"cmd\":\"check\"}", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"ok\":1"));
  TEST_ASSERT_TRUE(ota_check_called);
  // OTA preference persists across the (simulated) reboot.
  web_tick(g_mock_millis + 2000);  // flush the deferred save first
  web_setup(ctx);
  TEST_ASSERT_TRUE(ota.auto_enabled);
}

void test_update_upload(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  std::string fw(1500, '\0');
  for (size_t i = 0; i < fw.size(); i++) fw[i] = (char)(i * 31 + 7);
  WebServer::Resp r = WebServer::upload("/update", "firmware.bin", fw, auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(has(r.body, "UPDATE OK"));
  TEST_ASSERT_TRUE(Update.finished);
  TEST_ASSERT_EQUAL_INT((int)fw.size(), (int)Update.bytes.size());
  TEST_ASSERT_TRUE(Update.bytes == fw);
  TEST_ASSERT_TRUE(g_restart_requested);
  // Unauthed uploads are refused.
  fresh_env();
  r = WebServer::upload("/update", "firmware.bin", fw,
                        std::map<std::string, std::string>());
  TEST_ASSERT_EQUAL_INT(401, r.code);
}

void test_sta_uplink(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  TEST_ASSERT_EQUAL_INT(0, web_sta_state());  // AP-only by default
  TEST_ASSERT_FALSE(g_wifi_begun);
  WebServer::post("/api/admin",
                  "{\"sta_en\":1,\"sta_ssid\":\"Hot\",\"sta_pass\":\"pw\"}",
                  auth());
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(1, web_sta_state());  // connecting, non-blocking
  TEST_ASSERT_TRUE(g_wifi_begun);
  g_wifi_status = WL_CONNECTED;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(2, web_sta_state());
  WebServer::Resp r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"sta\":2"));
  // An attempt that never links falls back to AP-only after 30 s.
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::post("/api/admin", "{\"sta_en\":1,\"sta_ssid\":\"Hot\"}", auth());
  web_tick(g_mock_millis + 2000);
  web_setup(ctx);
  g_mock_millis += 31000;
  web_tick(g_mock_millis);
  TEST_ASSERT_EQUAL_INT(0, web_sta_state());
}

void test_save_coalescing(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  // Three rapid saves (like the dashboard's double-POST buttons): the bench
  // reacts instantly from RAM, but flash is untouched until the idle flush.
  WebServer::post("/api/config", "{\"step\":1000}", auth());
  TEST_ASSERT_EQUAL_INT(1000, cfg.step_delay_ms);  // RAM: instant
  WebServer::post("/api/config", "{\"step\":2000}", auth());
  WebServer::post("/api/spoof", "{\"cmd\":\"fire\",\"ssec\":6}", auth());
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
  cookie = login("admin", "admin123");
  WebServer::post("/api/config", "{\"step\":4321}", auth());
  TEST_ASSERT_EQUAL_INT(0, (int)Preferences::nvs_commits());
  // Reboot commits synchronously first: nothing lost, no tick needed.
  WebServer::post("/api/admin", "{\"cmd\":\"reboot\"}", auth());
  TEST_ASSERT_TRUE(g_restart_requested);
  TEST_ASSERT_EQUAL_INT(1, (int)Preferences::nvs_commits());
  cfg.step_delay_ms = 0;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(4321, cfg.step_delay_ms);
}

void test_industrial_config(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/config",
      "{\"loop\":1,\"cpause\":99999999,\"clim\":50,\"stag\":99999,\"dir\":5}",
      auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(cfg.loop_enabled);
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.cycle_pause_ms);  // capped
  TEST_ASSERT_EQUAL_INT(50, cfg.cycle_limit);
  TEST_ASSERT_EQUAL_INT(60000, cfg.allon_stagger_ms);     // capped
  TEST_ASSERT_EQUAL_INT(0, cfg.seq_dir);                  // bad -> forward
  WebServer::post("/api/config", "{\"dir\":1}", auth());
  TEST_ASSERT_EQUAL_INT(1, cfg.seq_dir);
  WebServer::post("/api/admin", "{\"auto\":1}", auth());
  TEST_ASSERT_TRUE(cfg.boot_autostart);
  web_tick(g_mock_millis + 2000);
  r = WebServer::get("/api/state", auth());
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
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/config", "{\"lbl0\":\"HORN\",\"lbl1\":\"LIGHT BAR\"}", auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  TEST_ASSERT_EQUAL_STRING("LIGHT BAR", cfg.relay_label[1]);
  web_tick(g_mock_millis + 2000);
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"lbl0\":\"HORN\""));
  // Hostile labels rejected: quotes (JSON/HTML break), backslash, too long.
  WebServer::post("/api/config", "{\"lbl0\":\"A\\\"B\"}", auth());
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  WebServer::post("/api/config", "{\"lbl0\":\"AAAAAAAAAAAAAAAA\"}", auth());
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
  WebServer::post("/api/config", "{\"lbl0\":\"A\\\\B\"}", auth());
  TEST_ASSERT_EQUAL_STRING("HORN", cfg.relay_label[0]);
}

void test_counters_state(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::post("/api/seq", "{\"cmd\":\"start\"}", auth());
  seq.tick(g_mock_millis + 3500);  // 8 sequential actuations done
  seq.tick(g_mock_millis + 3500 + 30000);  // hold_seq default -> cycle done
  TEST_ASSERT_EQUAL_UINT(1, seq.cyclesDone());
  WebServer::Resp r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"cycles\":1"));
  TEST_ASSERT_TRUE(has(r.body, "\"acts\":8"));
}

void run_all() {
  RUN_TEST(test_login_page_public);
  RUN_TEST(test_root_requires_auth);
  RUN_TEST(test_portal_redirect_flow);
  RUN_TEST(test_login_bad);
  RUN_TEST(test_login_good_dashboard);
  RUN_TEST(test_state_defaults);
  RUN_TEST(test_relay_override);
  RUN_TEST(test_seq_start_stop);
  RUN_TEST(test_config_validation_persist);
  RUN_TEST(test_spoof_fire_cancel);
  RUN_TEST(test_admin_validation);
  RUN_TEST(test_session_expiry_relogin);
  RUN_TEST(test_logout);
  RUN_TEST(test_fuzz_posts);
  RUN_TEST(test_factory_reset_clears);
  RUN_TEST(test_nvs_migration_v2);
  RUN_TEST(test_remember_login_persists);
  RUN_TEST(test_remember_expiry);
  RUN_TEST(test_token_eviction);
  RUN_TEST(test_relay_count_api);
  RUN_TEST(test_chase_runs_single);
  RUN_TEST(test_ota_api);
  RUN_TEST(test_update_upload);
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
