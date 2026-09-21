// Host execution of the REAL web_ui.cpp (v2.0 dashboard) against stubbed
// Arduino/WiFi/WebServer/Preferences (./Arduino.h etc.). Proves login,
// session, validation/clamping, NVS round-trip, API handlers and fuzz
// robustness without any ESP32. g++-only (see run_tests.sh); NOT in pio.
#include <unity.h>
#include <map>
#include <string>
#include "Arduino.h"
#include "WebServer.h"
#include "Preferences.h"
#include "../../src/relay_ctrl.h"
#include "../../src/web_ui.h"

void setUp(void) {}
void tearDown(void) {}

static Bms2Config cfg;
static RelaySequencer seq;
static SpoofWindow spoof;
static bool link_green = false;
static uint8_t spoofFrame[SPOOF_FRAME_LEN];
static void on_cfg() { build_spoof_frame(cfg, spoofFrame); }
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
  cfg = Bms2Config();
  seq.begin(&cfg);
  spoof.cancel();
  link_green = false;
  g_mock_millis = 1000000UL;
  g_restart_requested = false;
  ctx.cfg = &cfg;
  ctx.seq = &seq;
  ctx.spoof = &spoof;
  ctx.link_green = &link_green;
  ctx.on_config_changed = on_cfg;
  web_setup(ctx);
}

static std::string login(const char *u, const char *p) {
  std::map<std::string, std::string> args;
  args["u"] = u;
  args["p"] = p;
  WebServer::Resp r = WebServer::request(
      "POST", "/login", std::map<std::string, std::string>(), "", args);
  TEST_ASSERT_EQUAL_INT(302, r.code);
  auto it = r.headers.find("Set-Cookie");
  TEST_ASSERT_TRUE(it != r.headers.end());
  std::string sc = it->second;  // "BMS2=<tok>; Path=/"
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
  TEST_ASSERT_TRUE(has(r.body, "\"rmode\":0"));
  TEST_ASSERT_TRUE(has(r.body, "\"ssec\":10"));
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
      "{\"rmode\":1,\"step\":5,\"hold\":99999,\"bmode\":9,\"alow\":0}",
      auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_EQUAL_INT(RELAY_ALL_ON, cfg.relay_mode);
  TEST_ASSERT_EQUAL_INT(50, cfg.step_delay_ms);      // floored
  TEST_ASSERT_EQUAL_INT(3600, cfg.hold_seconds);     // capped
  TEST_ASSERT_EQUAL_INT(BTN_HOLD_ABORT, cfg.button_mode);  // bad -> default
  TEST_ASSERT_FALSE(cfg.active_low);
  // Persistence: trash RAM copy, reload from NVS.
  cfg.step_delay_ms = 12345;
  web_setup(ctx);
  TEST_ASSERT_EQUAL_INT(50, cfg.step_delay_ms);
  TEST_ASSERT_EQUAL_INT(3600, cfg.hold_seconds);
}

void test_spoof_fire_cancel(void) {
  fresh_env();
  cookie = login("admin", "admin123");
  WebServer::Resp r = WebServer::post(
      "/api/spoof",
      "{\"cmd\":\"fire\",\"sv\":888,\"sa\":888,\"sc\":888,\"ssoc\":188,\"ssec\":10}",
      auth());
  TEST_ASSERT_EQUAL_INT(200, r.code);
  TEST_ASSERT_TRUE(spoof.active(g_mock_millis));
  r = WebServer::get("/api/state", auth());
  TEST_ASSERT_TRUE(has(r.body, "\"spoof\":true"));
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
                       "/api/admin"};
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
  TEST_ASSERT_TRUE(cfg.hold_seconds <= 3600);
  TEST_ASSERT_TRUE(cfg.spoof_seconds >= 1 && cfg.spoof_seconds <= 120);
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
  TEST_ASSERT_EQUAL_INT(10, cfg.spoof_seconds);
}

void run_all() {
  RUN_TEST(test_login_page_public);
  RUN_TEST(test_root_requires_auth);
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
}

int main() {
  UNITY_BEGIN();
  run_all();
  return UNITY_END();
}
