#include "web_ui.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

static WebServer server(80);
static DNSServer dns;  // captive portal: offline AP, phones pop the login page
static Preferences nvs;
static WebCtx *G = nullptr;

// ---- persisted admin/AP identity (kept out of Bms2Config: strings) ----
struct WebIdentity {
  char ap_ssid[32] = WEB_AP_SSID_DEFAULT;
  char ap_pass[32] = WEB_AP_PASS_DEFAULT;
  uint8_t ap_channel = WEB_AP_CHANNEL_DEFAULT;
  char admin_user[24] = "admin";
  char admin_pass[32] = "admin123";
};
static WebIdentity ident;

// ---- session (RAM only) ----
static String session_tok = "";
static unsigned long session_until = 0;
#define SESSION_MS (30UL * 60UL * 1000UL)

static String make_token() {
  uint32_t a = (uint32_t)esp_random();
  uint32_t b = (uint32_t)esp_random();
  char buf[17];
  snprintf(buf, sizeof(buf), "%08lx%08lx", a, b);
  return String(buf);
}

static bool authed() {
  if (session_tok.length() == 0) return false;
  if (millis() > session_until) { session_tok = ""; return false; }
  if (!server.hasHeader("Cookie")) return false;
  String c = server.header("Cookie");
  if (c.indexOf("BMS2=" + session_tok) < 0) return false;
  session_until = millis() + SESSION_MS;  // sliding expiry
  return true;
}

// ---- NVS load/save ----
static void cfg_save() {
  if (!G || !G->cfg) return;
  Bms2Config &c = *G->cfg;
  nvs.begin(WEB_NVS_NS, false);
  nvs.putUChar("v", WEB_NVS_VERSION);
  nvs.putUShort("step", c.step_delay_ms);
  nvs.putUShort("hold", c.hold_seconds);
  nvs.putUChar("rmode", c.relay_mode);
  nvs.putUChar("bmode", c.button_mode);
  nvs.putBool("alow", c.active_low);
  nvs.putBool("binv", c.button_invert);
  nvs.putBool("sinv", c.spoof_invert);
  nvs.putBool("sena", c.spoof_enabled);
  nvs.putUShort("sv", c.spoof_v_tenth);
  nvs.putUShort("sa", c.spoof_a_tenth);
  nvs.putUShort("sc", c.spoof_c_tenth);
  nvs.putUChar("ssoc", c.spoof_soc);
  nvs.putUShort("ssec", c.spoof_seconds);
  nvs.putString("ap_ssid", ident.ap_ssid);
  nvs.putString("ap_pass", ident.ap_pass);
  nvs.putUChar("ap_ch", ident.ap_channel);
  nvs.putString("a_user", ident.admin_user);
  nvs.putString("a_pass", ident.admin_pass);
  nvs.end();
}

static void cfg_load() {
  if (!G || !G->cfg) return;
  Bms2Config &c = *G->cfg;
  nvs.begin(WEB_NVS_NS, true);
  if (nvs.getUChar("v", 0) != WEB_NVS_VERSION) { nvs.end(); return; }  // defaults
  c.step_delay_ms = nvs.getUShort("step", c.step_delay_ms);
  c.hold_seconds = nvs.getUShort("hold", c.hold_seconds);
  c.relay_mode = nvs.getUChar("rmode", c.relay_mode);
  c.button_mode = nvs.getUChar("bmode", c.button_mode);
  c.active_low = nvs.getBool("alow", c.active_low);
  c.button_invert = nvs.getBool("binv", c.button_invert);
  c.spoof_invert = nvs.getBool("sinv", c.spoof_invert);
  c.spoof_enabled = nvs.getBool("sena", c.spoof_enabled);
  c.spoof_v_tenth = nvs.getUShort("sv", c.spoof_v_tenth);
  c.spoof_a_tenth = nvs.getUShort("sa", c.spoof_a_tenth);
  c.spoof_c_tenth = nvs.getUShort("sc", c.spoof_c_tenth);
  c.spoof_soc = nvs.getUChar("ssoc", c.spoof_soc);
  c.spoof_seconds = nvs.getUShort("ssec", c.spoof_seconds);
  String s = nvs.getString("ap_ssid", ident.ap_ssid);
  s.toCharArray(ident.ap_ssid, sizeof(ident.ap_ssid));
  s = nvs.getString("ap_pass", ident.ap_pass);
  s.toCharArray(ident.ap_pass, sizeof(ident.ap_pass));
  ident.ap_channel = nvs.getUChar("ap_ch", ident.ap_channel);
  s = nvs.getString("a_user", ident.admin_user);
  s.toCharArray(ident.admin_user, sizeof(ident.admin_user));
  s = nvs.getString("a_pass", ident.admin_pass);
  s.toCharArray(ident.admin_pass, sizeof(ident.admin_pass));
  nvs.end();
  if (ident.ap_channel < 1 || ident.ap_channel > 13) ident.ap_channel = 6;
}

void web_factory_reset() {
  nvs.begin(WEB_NVS_NS, false);
  nvs.clear();
  nvs.end();
  delay(200);
  ESP.restart();
}

// ---- pages ----
static const char PAGE_LOGIN[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>BMS Tester login</title><style>body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#e2e8f0}form{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:24px}input{width:100%;background:#0f172a;border:1px solid #475569;color:#e2e8f0;border-radius:8px;padding:10px;margin:4px 0 12px}input[type=submit]{background:#2563eb;border:0;font-size:15px;cursor:pointer}</style></head><body style="max-width:380px;margin:60px auto;padding:12px">
<h2>BMS Tester — login</h2>
<form method=POST action=/login>
User<br><input name=u style="width:100%"><br><br>
Password<br><input name=p type=password style="width:100%"><br><br>
<input type=submit value=Login>
</form></body></html>)HTML";

static const char PAGE_DASH[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>BMS Tester</title><style>
*{box-sizing:border-box}body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:12px}
.wrap{max-width:720px;margin:0 auto}header{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:4px}
header h2{margin:0;font-size:22px}.ver{font-size:12px;color:#94a3b8}
.pill{display:inline-block;padding:3px 12px;border-radius:999px;font-size:13px;font-weight:700}
#link.G{background:#14532d;color:#4ade80}#link.R{background:#450a0a;color:#f87171}#clk{color:#94a3b8;font-size:13px}
a{color:#60a5fa}.card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:14px;margin:12px 0}
.card h3{margin:0 0 10px;font-size:15px;color:#93c5fd;text-transform:uppercase;letter-spacing:.5px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin:10px 0}
.rly{border:2px solid #475569;background:#0f172a;color:#94a3b8;border-radius:10px;padding:14px 4px;font-size:15px;font-weight:700;cursor:pointer}
.rly small{display:block;font-size:11px;font-weight:400}
.rly.on{border-color:#22c55e;background:#052e16;color:#4ade80;box-shadow:0 0 12px #22c55e66}
button,.btn{background:#2563eb;border:0;color:#fff;border-radius:8px;padding:9px 14px;font-size:14px;cursor:pointer;margin:2px}
button.danger{background:#b91c1c}button.warn{background:#b45309}button.ok{background:#15803d}
label{font-size:13px;color:#cbd5e1}input,select{background:#0f172a;border:1px solid #475569;color:#e2e8f0;border-radius:8px;padding:8px;margin:3px 2px;font-size:14px}
.row{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin:4px 0}.msg{font-size:13px;color:#4ade80;margin-left:8px}
.spoof-on{color:#fbbf24;font-weight:700}
</style></head><body><div class=wrap>
<header><h2>&#9889; BMS Tester</h2><span class=ver>v2.0</span><span id=link class="pill R">?</span><span id=clk></span><span style="flex:1"></span><a href=/logout>Logout</a></header>
<div class=card><h3>Relays</h3>
<div class=row><button class=ok onclick="seq('start')">&#9654; START</button><button class=danger onclick="seq('stop')">STOP ALL</button></div>
<div class=grid id=relays></div></div>
<div class=card><h3>Sequence config</h3>
<div class=row><label>Mode <select id=rmode><option value=0>Sequential 1-8</option><option value=1>All ON at once</option></select></label>
<label>Step ms <input id=step size=6></label>
<label>Hold s (0=&infin;) <input id=hold size=6></label></div>
<div class=row><label>Button <select id=bmode><option value=0>Hold X s, re-press=OFF</option><option value=1>Run to end, ignore presses</option><option value=2>Re-press restarts</option></select></label>
<label>Logic <select id=alow><option value=1>Active-LOW (SmartElex)</option><option value=0>Active-HIGH</option></select></label>
<button onclick="saveCfg()">Save</button><span class=msg id=cfgmsg></span></div></div>
<div class=card><h3>Fault spoof (0x03 test values)</h3>
<div class=row><label><input type=checkbox id=sena> pin-trigger enabled</label><span id=spoofmsg class=spoof-on></span></div>
<div class=row><label>V <input id=sv size=5></label><label>A <input id=sa size=5></label><label>&deg;C <input id=sc size=5></label><label>SOC% <input id=ssoc size=4></label><label>Secs <input id=ssec size=4></label></div>
<div class=row><button class=warn onclick="spoof('fire')">FIRE now</button><button onclick="spoof('cancel')">Cancel</button></div></div>
<div class=card><h3>Admin &amp; Wi-Fi AP</h3>
<div class=row><label>SSID <input id=ap_ssid size=14></label><label>Pass (8+) <input id=ap_pass size=14></label><label>Ch <input id=ap_ch size=3></label></div>
<div class=row><label>User <input id=a_user size=10></label><label>New pass <input id=a_pass type=password size=14></label></div>
<div class=row><button onclick="saveAdmin()">Save</button>
<button class=warn onclick="if(confirm('Reboot?'))admin('reboot')">Reboot</button>
<button class=danger onclick="if(confirm('Factory reset?'))admin('reset')">Factory reset</button><span class=msg id=admmsg></span></div></div>
</div>
<script>
async function jget(u){let r=await fetch(u);return r.json();}
async function jpost(u,b){let r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});return r.json();}
async function refresh(){let s=await jget('/api/state');
 let lk=document.getElementById('link');lk.textContent=s.link?'LINK GREEN':'LINK RED';lk.className='pill '+(s.link?'G':'R');
 document.getElementById('clk').textContent='seq '+(s.running?'RUNNING':'IDLE');
 document.getElementById('spoofmsg').textContent=s.spoof?'SPOOF ACTIVE':'';
 let d=document.getElementById('relays');d.innerHTML='';
 s.relays.forEach((on,i)=>{d.innerHTML+=`<button class="rly${on?' on':''}" onclick="relay(${i},${on?0:1})">R${i+1}<small>${on?'ON':'OFF'}</small></button>`;});
 for(let k of ['rmode','step','hold','bmode','alow','sena','sv','sa','sc','ssoc','ssec','ap_ssid','ap_pass','ap_ch','a_user']){let e=document.getElementById(k);if(e&&document.activeElement!==e){e.value=s.cfg[k];if(k==='sena')e.checked=!!s.cfg[k];}}
}
async function relay(i,on){await jpost('/api/relay',{i,on});refresh();}
async function seq(c){await jpost('/api/seq',{cmd:c});refresh();}
async function saveCfg(){let b={};for(let k of ['rmode','step','hold','bmode','alow']){b[k]=+document.getElementById(k).value;}let r=await jpost('/api/config',b);document.getElementById('cfgmsg').textContent=r.ok?'saved':'ERR';}
async function spoof(c){let b={cmd:c};if(c==='fire'){for(let k of ['sv','sa','sc','ssoc','ssec'])b[k]=+document.getElementById(k).value;b.sena=document.getElementById('sena').checked?1:0;}let r=await jpost('/api/spoof',b);document.getElementById('spoofmsg').textContent=r.ok?'ok':'ERR';refresh();}
async function saveAdmin(){let b={};for(let k of ['ap_ssid','ap_pass','ap_ch','a_user','a_pass'])b[k]=document.getElementById(k).value;let r=await jpost('/api/admin',b);document.getElementById('admmsg').textContent=r.ok?'saved (reboot to apply AP)':'ERR: '+r.err;}
async function admin(c){await jpost('/api/admin',{cmd:c});}
setInterval(refresh,1000);refresh();
</script></body></html>)HTML";

// ---- route helpers ----
static void send_json(const String &s) {
  server.send(200, "application/json", s);
}

static void handle_root() {
  if (!authed()) { server.sendHeader("Location", "/login"); server.send(302); return; }
  server.send_P(200, "text/html", PAGE_DASH);
}

static void handle_login_page() {
  server.send_P(200, "text/html", PAGE_LOGIN);
}

static void handle_login() {
  String u = server.arg("u"), p = server.arg("p");
  if (u == ident.admin_user && p == ident.admin_pass) {
    session_tok = make_token();
    session_until = millis() + SESSION_MS;
    server.sendHeader("Set-Cookie", "BMS2=" + session_tok + "; Path=/");
    server.sendHeader("Location", "/");
    server.send(302);
  } else {
    delay(500);  // slow brute force
    server.send(401, "text/plain", "bad login");
  }
}

static void handle_logout() {
  session_tok = "";
  server.sendHeader("Location", "/login");
  server.send(302);
}

static void handle_state() {
  if (!authed()) { server.send(401); return; }
  Bms2Config &c = *G->cfg;
  String s = "{\"link\":";
  s += (*G->link_green ? "true" : "false");
  s += ",\"running\":";
  s += (G->seq->running() ? "true" : "false");
  s += ",\"spoof\":";
  s += (G->spoof->active(millis()) ? "true" : "false");
  s += ",\"relays\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (i) s += ",";
    s += (G->seq->relayOn(i) ? "1" : "0");
  }
  s += "],\"cfg\":{\"rmode\":" + String(c.relay_mode) +
       ",\"step\":" + String(c.step_delay_ms) +
       ",\"hold\":" + String(c.hold_seconds) +
       ",\"bmode\":" + String((int)c.button_mode) +
       ",\"alow\":" + String(c.active_low ? 1 : 0) +
       ",\"sena\":" + String(c.spoof_enabled ? 1 : 0) +
       ",\"sv\":" + String(c.spoof_v_tenth) +
       ",\"sa\":" + String(c.spoof_a_tenth) +
       ",\"sc\":" + String(c.spoof_c_tenth) +
       ",\"ssoc\":" + String(c.spoof_soc) +
       ",\"ssec\":" + String(c.spoof_seconds) +
       ",\"ap_ssid\":\"" + String(ident.ap_ssid) +
       "\",\"ap_pass\":\"" + String(ident.ap_pass) +
       "\",\"ap_ch\":" + String(ident.ap_channel) +
       ",\"a_user\":\"" + String(ident.admin_user) + "\"}}";
  send_json(s);
}

// Minimal JSON int/string getters (no extra library).
static long jnum(const String &b, const char *key, long dflt) {
  String k = String("\"") + key + "\":";
  int i = b.indexOf(k);
  if (i < 0) return dflt;
  return b.substring(i + k.length()).toInt();
}
static String jstr(const String &b, const char *key) {
  String k = String("\"") + key + "\":\"";
  int i = b.indexOf(k);
  if (i < 0) return "";
  int j = b.indexOf('"', i + k.length());
  if (j < 0) return "";
  return b.substring(i + k.length(), j);
}
static bool has(const String &b, const char *key) {
  return b.indexOf(String("\"") + key + "\"") >= 0;
}

static void handle_relay() {
  if (!authed()) { server.send(401); return; }
  String b = server.arg("plain");
  int i = (int)jnum(b, "i", -1);
  if (i < 0 || i >= RELAY_COUNT) { server.send(400); return; }
  G->seq->setForced((uint8_t)i, jnum(b, "on", 0) != 0);
  send_json("{\"ok\":1}");
}

static void handle_seq() {
  if (!authed()) { server.send(401); return; }
  String b = server.arg("plain");
  if (jstr(b, "cmd") == "start") G->seq->start(millis());
  else G->seq->stopAll();
  send_json("{\"ok\":1}");
}

static void handle_config() {
  if (!authed()) { server.send(401); return; }
  Bms2Config &c = *G->cfg;
  String b = server.arg("plain");
  if (has(b, "rmode")) c.relay_mode = jnum(b, "rmode", 0) ? RELAY_ALL_ON : RELAY_SEQUENTIAL;
  if (has(b, "step")) c.step_delay_ms = (uint16_t)constrain(jnum(b, "step", 500), 50, 60000);
  if (has(b, "hold")) c.hold_seconds = (uint16_t)constrain(jnum(b, "hold", 30), 0, 3600);
  if (has(b, "bmode")) {
    long m = jnum(b, "bmode", 0);
    c.button_mode = (m < 0 || m > 2) ? BTN_HOLD_ABORT : (uint8_t)m;
  }
  if (has(b, "alow")) c.active_low = jnum(b, "alow", 1) != 0;
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  send_json("{\"ok\":1}");
}

static void handle_spoof() {
  if (!authed()) { server.send(401); return; }
  Bms2Config &c = *G->cfg;
  String b = server.arg("plain");
  String cmd = jstr(b, "cmd");
  if (cmd == "cancel") { G->spoof->cancel(); send_json("{\"ok\":1}"); return; }
  // fire: optional inline values (x0.1 display units), then trigger window.
  if (has(b, "sv")) c.spoof_v_tenth = (uint16_t)constrain(jnum(b, "sv", 888), 0, 9999);
  if (has(b, "sa")) c.spoof_a_tenth = (uint16_t)constrain(jnum(b, "sa", 888), 0, 9999);
  if (has(b, "sc")) c.spoof_c_tenth = (uint16_t)constrain(jnum(b, "sc", 888), 0, 9999);
  if (has(b, "ssoc")) c.spoof_soc = (uint8_t)constrain(jnum(b, "ssoc", 188), 0, 255);
  if (has(b, "ssec")) c.spoof_seconds = (uint16_t)constrain(jnum(b, "ssec", 10), 1, 120);
  if (has(b, "sena")) c.spoof_enabled = jnum(b, "sena", 1) != 0;
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  if (cmd == "fire")
    G->spoof->trigger(millis(), (unsigned long)c.spoof_seconds * 1000UL);
  send_json("{\"ok\":1}");
}

static void handle_admin() {
  if (!authed()) { server.send(401); return; }
  String b = server.arg("plain");
  String cmd = jstr(b, "cmd");
  if (cmd == "reset") { send_json("{\"ok\":1}"); web_factory_reset(); return; }
  if (cmd == "reboot") { send_json("{\"ok\":1}"); delay(200); ESP.restart(); return; }
  if (has(b, "ap_ssid")) {
    String v = jstr(b, "ap_ssid");
    if (v.length() > 0 && v.length() < (int)sizeof(ident.ap_ssid)) {
      v.toCharArray(ident.ap_ssid, sizeof(ident.ap_ssid));
    }
  }
  if (has(b, "ap_pass")) {
    String v = jstr(b, "ap_pass");
    if (v.length() >= 8 && v.length() < (int)sizeof(ident.ap_pass)) {
      v.toCharArray(ident.ap_pass, sizeof(ident.ap_pass));
    } else if (v.length() > 0) {
      send_json("{\"ok\":0,\"err\":\"AP password needs 8+ chars\"}");
      return;
    }
  }
  if (has(b, "ap_ch")) {
    long ch = jnum(b, "ap_ch", 6);
    if (ch >= 1 && ch <= 13) ident.ap_channel = (uint8_t)ch;
  }
  if (has(b, "a_user")) {
    String v = jstr(b, "a_user");
    if (v.length() > 0 && v.length() < (int)sizeof(ident.admin_user))
      v.toCharArray(ident.admin_user, sizeof(ident.admin_user));
  }
  if (has(b, "a_pass")) {
    String v = jstr(b, "a_pass");
    if (v.length() >= 4 && v.length() < (int)sizeof(ident.admin_pass))
      v.toCharArray(ident.admin_pass, sizeof(ident.admin_pass));
    else if (v.length() > 0) {
      send_json("{\"ok\":0,\"err\":\"admin password needs 4+ chars\"}");
      return;
    }
  }
  cfg_save();
  send_json("{\"ok\":1}");
}

void web_setup(WebCtx &ctx) {
  G = &ctx;
  // Start from compiled defaults, then overlay NVS. (On real hardware the
  // statics already hold defaults at boot, so this is a no-op there; it
  // also makes repeated setup calls deterministic.)
  strncpy(ident.ap_ssid, WEB_AP_SSID_DEFAULT, sizeof(ident.ap_ssid));
  strncpy(ident.ap_pass, WEB_AP_PASS_DEFAULT, sizeof(ident.ap_pass));
  ident.ap_channel = WEB_AP_CHANNEL_DEFAULT;
  strncpy(ident.admin_user, "admin", sizeof(ident.admin_user));
  strncpy(ident.admin_pass, "admin123", sizeof(ident.admin_pass));
  cfg_load();  // NVS -> cfg + identity (or defaults)
  if (G->on_config_changed) G->on_config_changed();  // build spoof frame
  // AP ALWAYS ON: no STA, no timeouts, works with zero office network.
  // Fixed 192.168.4.1 gateway (documented everywhere) + DNS catch-all so
  // any URL on the device resolves to us (captive portal).
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(ident.ap_ssid, ident.ap_pass, ident.ap_channel);
  dns.start(53, "*", WiFi.softAPIP());
  static const char *HDRS[] = {"Cookie"};
  server.collectHeaders(HDRS, 1);
  server.on("/", handle_root);
  server.on("/login", HTTP_GET, handle_login_page);
  server.on("/login", HTTP_POST, handle_login);
  server.on("/logout", handle_logout);
  server.on("/api/state", HTTP_GET, handle_state);
  server.on("/api/relay", HTTP_POST, handle_relay);
  server.on("/api/seq", HTTP_POST, handle_seq);
  server.on("/api/config", HTTP_POST, handle_config);
  server.on("/api/spoof", HTTP_POST, handle_spoof);
  server.on("/api/admin", HTTP_POST, handle_admin);
  // Captive portal: any unknown host/path lands on "/" (which itself sends
  // unauthed browsers to /login). This makes phones pop the login page on
  // join and keeps "no internet" devices from showing a dead 404.
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302);
  });
  server.begin();
}

void web_tick(unsigned long now) {
  (void)now;
  dns.processNextRequest();  // captive portal DNS (cheap; no-op off-AP)
  server.handleClient();     // short, non-blocking; RS485 keeps priority
}

#endif  // ARDUINO
