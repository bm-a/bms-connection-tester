#include "web_ui.h"
#include "fw_upload.h"  // v2.4 Tasmota-grade update gates (host-tested)

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <ESPmDNS.h>  // v2.4 bmstester.local (laptops/desktops on the AP)
#include <esp_system.h>  // esp_reset_reason() for the info card
#else
// Host stubs for the extra ESP APIs used below (test_web ONLY).
#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "DNSServer.h"
#include "Preferences.h"
#include "Update.h"
#include "ESPmDNS.h"
#include "esp_system.h"
#endif

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
  // v2.3 optional STA uplink (phone hotspot): ONLY used for GitHub OTA
  // checks. Empty SSID or sta_en=false = AP-only, as before.
  char sta_ssid[32] = "";
  char sta_pass[64] = "";
  bool sta_en = false;
  // v2.4 custom firmware URL (Tasmota OtaUrl equivalent): overrides the
  // GitHub release asset for "Upgrade from URL". Empty = GitHub default.
  char ota_url[128] = "";
};
static WebIdentity ident;

// ---- v2.3.1: no login wall. The dashboard is open to anyone on the AP
// (WPA2 password is the gate). Sensitive actions (/update, /api/admin,
// /api/ota) carry the admin password per request instead of a session.
static bool admin_ok(const String &pass) {
  return ident.admin_pass[0] != '\0' && pass == ident.admin_pass;
}

// ---- NVS load/save (v3; migrates v2 boxes in place) ----
// v2.3 latency fix: handlers only MARK dirty (RAM is already updated, so the
// bench reacts instantly); web_tick() commits to flash once after 1.5 s idle.
// Rapid Save-taps coalesce into ONE flash write instead of stalling loop()
// (and the 9600-baud path) on every POST.
static bool cfg_dirty = false;
static unsigned long cfg_dirty_at = 0;
#define CFG_FLUSH_MS 1500UL

static void cfg_commit() {
  if (!G || !G->cfg) return;
  Bms2Config &c = *G->cfg;
  nvs.begin(WEB_NVS_NS, false);
  nvs.putUChar("v", WEB_NVS_VERSION);
  nvs.putUShort("step", c.step_delay_ms);
  nvs.putUInt("hseq", c.hold_seq_ms);  // v3: per-mode holds, ms
  nvs.putUChar("swp", c.chase_sweeps);  // v2.3.1: chase auto-hold sweeps
  nvs.putUInt("hall", c.hold_all_ms);
  nvs.putUChar("nrel", c.relay_count);
  nvs.putUChar("rmode", c.relay_mode);
  nvs.putUChar("bmode", c.button_mode);
  nvs.putBool("loop", c.loop_enabled);
  nvs.putUInt("cpause", c.cycle_pause_ms);
  nvs.putUShort("clim", c.cycle_limit);
  nvs.putUShort("stag", c.allon_stagger_ms);
  nvs.putUChar("dir", c.seq_dir);
  nvs.putBool("auto", c.boot_autostart);
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    char k[8];
    snprintf(k, sizeof(k), "rlbl%u", i);
    nvs.putString(k, c.relay_label[i]);
  }
  nvs.putBool("alow", c.active_low);
  nvs.putBool("binv", c.button_invert);
  nvs.putBool("sinv", c.spoof_invert);
  nvs.putBool("sena", c.spoof_enabled);
  nvs.putUChar("spin", sanitize_spoof_pin(c.spoof_pin));  // v2.3.1 trigger GPIO
  nvs.putUShort("sv", c.spoof_v_tenth);
  nvs.putUShort("sa", c.spoof_a_tenth);
  nvs.putUShort("sc", c.spoof_c_tenth);
  nvs.putUChar("ssoc", c.spoof_soc);
  nvs.putUShort("ssec", c.spoof_seconds);
  nvs.putUShort("s2v", c.s2_v_tenth);
  nvs.putUShort("s2a", c.s2_a_tenth);
  nvs.putUShort("s2c", c.s2_c_tenth);
  nvs.putUChar("s2soc", c.s2_soc);
  nvs.putUShort("s2sec", c.s2_seconds);
  nvs.putString("ap_ssid", ident.ap_ssid);
  nvs.putString("ap_pass", ident.ap_pass);
  nvs.putUChar("ap_ch", ident.ap_channel);
  nvs.putString("a_user", ident.admin_user);
  nvs.putString("a_pass", ident.admin_pass);
  nvs.putString("sta_ssid", ident.sta_ssid);
  nvs.putString("sta_pass", ident.sta_pass);
  nvs.putBool("sta_en", ident.sta_en);
  nvs.putString("ota_url", ident.ota_url);  // v2.4 custom firmware URL
  if (G->ota) {
    nvs.putBool("ota_auto", G->ota->auto_enabled);
    nvs.putUInt("ota_int", G->ota->interval_ms);
  }
  nvs.end();
}

static void cfg_save() {
  cfg_dirty = true;
  cfg_dirty_at = millis();
}

static void cfg_flush(unsigned long now) {
  if (!cfg_dirty) return;
  if ((now - cfg_dirty_at) < CFG_FLUSH_MS) return;  // coalesce rapid saves
  cfg_dirty = false;
  cfg_commit();
}

static void cfg_load() {
  if (!G || !G->cfg) return;
  Bms2Config &c = *G->cfg;
  nvs.begin(WEB_NVS_NS, true);
  uint8_t v = nvs.getUChar("v", 0);
  if (v != WEB_NVS_VERSION && v != 2) {
    nvs.end();
    return;  // factory defaults
  }
  c.step_delay_ms = nvs.getUShort("step", c.step_delay_ms);
  c.relay_mode = nvs.getUChar("rmode", c.relay_mode);
  c.button_mode = nvs.getUChar("bmode", c.button_mode);
  c.active_low = nvs.getBool("alow", c.active_low);
  c.button_invert = nvs.getBool("binv", c.button_invert);
  c.spoof_invert = nvs.getBool("sinv", c.spoof_invert);
  c.spoof_enabled = nvs.getBool("sena", c.spoof_enabled);
  c.spoof_pin = sanitize_spoof_pin(nvs.getUChar("spin", c.spoof_pin));
  if (v == 2) {
    // ---- migrate a v2 box ----
    // hold: seconds -> ms, fanned out to all three per-mode holds.
    uint32_t hold_ms = (uint32_t)nvs.getUShort("hold", 30) * 1000UL;
    c.hold_seq_ms = hold_ms;
    c.hold_all_ms = hold_ms;  // chase: v2 boxes keep default 3 auto-sweeps
    // singles -> STAGE 2 (user's custom values keep working, now second);
    // stage 1 = "100 first" defaults (already in the struct defaults).
    c.s2_v_tenth = nvs.getUShort("sv", c.s2_v_tenth);
    c.s2_a_tenth = nvs.getUShort("sa", c.s2_a_tenth);
    c.s2_c_tenth = nvs.getUShort("sc", c.s2_c_tenth);
    c.s2_soc = nvs.getUChar("ssoc", c.s2_soc);
    c.s2_seconds = nvs.getUShort("ssec", c.s2_seconds);
  } else {
    c.hold_seq_ms = nvs.getUInt("hseq", c.hold_seq_ms);
    c.chase_sweeps = nvs.getUChar("swp", c.chase_sweeps);
    c.hold_all_ms = nvs.getUInt("hall", c.hold_all_ms);
    c.relay_count = nvs.getUChar("nrel", c.relay_count);
    c.loop_enabled = nvs.getBool("loop", c.loop_enabled);
    c.cycle_pause_ms = nvs.getUInt("cpause", c.cycle_pause_ms);
    c.cycle_limit = nvs.getUShort("clim", c.cycle_limit);
    c.allon_stagger_ms = nvs.getUShort("stag", c.allon_stagger_ms);
    c.seq_dir = nvs.getUChar("dir", c.seq_dir);
    c.boot_autostart = nvs.getBool("auto", c.boot_autostart);
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
      char k[8];
      snprintf(k, sizeof(k), "rlbl%u", i);
      String v = nvs.getString(k, c.relay_label[i]);
      v.toCharArray(c.relay_label[i], sizeof(c.relay_label[i]));
    }
    c.spoof_v_tenth = nvs.getUShort("sv", c.spoof_v_tenth);
    c.spoof_a_tenth = nvs.getUShort("sa", c.spoof_a_tenth);
    c.spoof_c_tenth = nvs.getUShort("sc", c.spoof_c_tenth);
    c.spoof_soc = nvs.getUChar("ssoc", c.spoof_soc);
    c.spoof_seconds = nvs.getUShort("ssec", c.spoof_seconds);
    c.s2_v_tenth = nvs.getUShort("s2v", c.s2_v_tenth);
    c.s2_a_tenth = nvs.getUShort("s2a", c.s2_a_tenth);
    c.s2_c_tenth = nvs.getUShort("s2c", c.s2_c_tenth);
    c.s2_soc = nvs.getUChar("s2soc", c.s2_soc);
    c.s2_seconds = nvs.getUShort("s2sec", c.s2_seconds);
    c.spoof_pin = sanitize_spoof_pin(nvs.getUChar("spin", c.spoof_pin));
    if (G->ota) {
      G->ota->auto_enabled = nvs.getBool("ota_auto", G->ota->auto_enabled);
      G->ota->interval_ms = nvs.getUInt("ota_int", G->ota->interval_ms);
    }
  }
  String s = nvs.getString("ap_ssid", ident.ap_ssid);
  s.toCharArray(ident.ap_ssid, sizeof(ident.ap_ssid));
  s = nvs.getString("ap_pass", ident.ap_pass);
  s.toCharArray(ident.ap_pass, sizeof(ident.ap_pass));
  ident.ap_channel = nvs.getUChar("ap_ch", ident.ap_channel);
  s = nvs.getString("a_user", ident.admin_user);
  s.toCharArray(ident.admin_user, sizeof(ident.admin_user));
  s = nvs.getString("a_pass", ident.admin_pass);
  s.toCharArray(ident.admin_pass, sizeof(ident.admin_pass));
  s = nvs.getString("sta_ssid", ident.sta_ssid);
  s.toCharArray(ident.sta_ssid, sizeof(ident.sta_ssid));
  s = nvs.getString("sta_pass", ident.sta_pass);
  s.toCharArray(ident.sta_pass, sizeof(ident.sta_pass));
  ident.sta_en = nvs.getBool("sta_en", ident.sta_en);
  s = nvs.getString("ota_url", ident.ota_url);
  s.toCharArray(ident.ota_url, sizeof(ident.ota_url));
  nvs.end();
  if (ident.ap_channel < 1 || ident.ap_channel > 13) ident.ap_channel = 6;
  // Clamp everything a hand-edited NVS (or older UI) could have stored.
  // v2.4 floors (R4/R5/R6) enforced here too, so old boxes self-heal.
  if (c.relay_count < 1 || c.relay_count > RELAY_COUNT) c.relay_count = 8;
  if (c.relay_mode > RELAY_CHASE) c.relay_mode = RELAY_SEQUENTIAL;
  if (c.button_mode > BTN_RESTART) c.button_mode = BTN_HOLD_ABORT;
  if (c.seq_dir > 1) c.seq_dir = 0;
  if (c.step_delay_ms < 100 || c.step_delay_ms > 60000) c.step_delay_ms = 250;
  if (c.cycle_pause_ms < 500 || c.cycle_pause_ms > 60000)
    c.cycle_pause_ms = 2000;
  if (c.allon_stagger_ms > 1000) c.allon_stagger_ms = 50;
}

void web_factory_reset() {
  cfg_dirty = false;
  nvs.begin(WEB_NVS_NS, false);
  nvs.clear();
  nvs.end();
  // NOTE: deliberately NOT web_reboot_now() — that commits RAM to NVS first,
  // which would resurrect the just-wiped settings. Wipe, grace, restart.
  delay(500);
  ESP.restart();
}

// v2.4: every reboot path funnels through here (Tasmota Restart-1 spirit):
// pending NVS saves are flushed first (a reboot wipes RAM), then a grace
// beat so the HTTP reply actually leaves before the radio dies.
void web_reboot_now() {
  cfg_commit();
  delay(500);
  ESP.restart();
}

// v2.4 device identity: boot counter + reset reason (Tasmota Status 1/2).
static unsigned web_boot_count = 0;
static char web_reset_reason[40] = "unknown";

// Reset-reason vocabulary (Tasmota Status-style short names).
static const char *reset_reason_str(int r) {
  switch (r) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int-wdt";
    case ESP_RST_TASK_WDT: return "task-wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

static void web_boot_track() {
  web_boot_count = 0;
  nvs.begin(WEB_NVS_NS, true);
  // NOTE: no version gate — the key is independent of the config schema.
  web_boot_count = nvs.getUInt("bootn", 0) + 1;
  nvs.end();
  nvs.begin(WEB_NVS_NS, false);
  nvs.putUInt("bootn", web_boot_count);
  nvs.end();
  strncpy(web_reset_reason, reset_reason_str((int)esp_reset_reason()),
          sizeof(web_reset_reason) - 1);
  web_reset_reason[sizeof(web_reset_reason) - 1] = '\0';
}

void web_bootcount_reset() {  // Tasmota Reset-99 equivalent (no reboot)
  web_boot_count = 0;
  nvs.begin(WEB_NVS_NS, false);
  nvs.putUInt("bootn", 0);
  nvs.end();
}

// ---- pages ----
static const char PAGE_DASH[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
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
.rly.lim{opacity:.35;cursor:not-allowed}
.note{font-size:12px;color:#94a3b8}
</style></head><body><div class=wrap>
<header><h2>&#9889; BMS Tester</h2><span class=ver id=fwver></span><span id=link class="pill R">?</span><span id=clk></span></header>
<div class=card><h3>Relays</h3>
<div class=row><button class=ok onclick="seq('start')">&#9654; START</button><button class=danger onclick="seq('stop')">STOP ALL</button></div>
<div class=grid id=relays></div></div>
<div class=card><h3>Sequence config</h3>
<div class=row><label>Mode <select id=rmode onchange="showMode()"><option value=0>Sequential 1-N</option><option value=2>Chase wave</option><option value=1>All ON at once</option></select></label>
<label>Relays <input id=nrel size=3 title="1-8: first N relays take part"></label></div>
<div class=row id=row_step><label>Step ms <input id=step size=6 title="100-60000, gap between relays"></label></div>
<div class=row id=row_seqonly><label>Hold sequential ms (0=stay ON) <input id=hseq size=7></label></div>
<div class=row id=row_chaseonly><label>Chase sweeps (0=forever) <input id=swp size=4 title="auto-hold = sweeps x relays x step"></label></div>
<div class=row id=row_allonly><label>Hold all-on ms (0=stay ON) <input id=hall size=7></label>
<label>All-ON stagger ms <input id=stag size=5 title="0 = all at once (contactor slam); 20-1000 ramps the inrush"></label></div>
<div class=row id=row_dir><label>Direction <select id=dir><option value=0>R1&rarr;Rn</option><option value=1>Rn&rarr;R1</option></select></label></div>
<div class=row><label>Button <select id=bmode><option value=0>Hold X ms, re-press=OFF</option><option value=1>Run to end, ignore presses</option><option value=2>Re-press restarts</option></select></label>
<label>Logic <select id=alow><option value=1>Active-LOW (SmartElex)</option><option value=0>Active-HIGH</option></select></label></div>
<div class=row><label><input type=checkbox id=loop> Loop cycles</label>
<label>Pause ms <input id=cpause size=7 title="500-60000, coil cooling between cycles"></label>
<label>Cycle limit (0=&infin;) <input id=clim size=5></label></div>
<div class=row><button onclick="saveCfg()">Save</button><span class=msg id=cfgmsg></span></div>
<div class=row><span class=note>Only the active mode's fields are shown. Mode switch needs STOP first; loop needs a finite hold; START is refused for 0.5 s after STOP (relay settle).</span></div></div>
<div class=card><h3>Relay labels</h3>
<div class=row id=labels></div>
<div class=row><button onclick="saveLabels()">Save labels</button><span class=msg id=lblmsg></span></div></div>
<div class=card><h3>Fault spoof (0x03 test values, stage 1 then 2)</h3>
<div class=row><label><input type=checkbox id=sena> pin-trigger enabled</label><label>Trigger GPIO <input id=spin size=3></label><span id=spoofmsg class=spoof-on></span></div>
<div class=row><span class=note>GPIO + values save on FIRE. Safe pins: 1, 2, 21, 38-44, 47 (else 21).</span></div>
<div class=row><label>1: V <input id=sv size=5></label><label>A <input id=sa size=5></label><label>&deg;C <input id=sc size=5></label><label>SOC% <input id=ssoc size=4></label><label>Secs <input id=ssec size=4></label></div>
<div class=row><label>2: V <input id=s2v size=5></label><label>A <input id=s2a size=5></label><label>&deg;C <input id=s2c size=5></label><label>SOC% <input id=s2soc size=4></label><label>Secs <input id=s2sec size=4></label></div>
<div class=row><button class=warn onclick="spoof('fire')">FIRE now</button><button onclick="spoof('save')">Save only</button><button onclick="spoof('cancel')">Cancel</button><span class=msg id=spoofsave></span></div>
<div class=row><span class=note>Save only stages values + pin without firing. FIRE saves then fires.</span></div></div>
<div class=card><h3>Firmware update</h3>
<div class=row><span id=ota_status class=note></span><span id=ota_latest class=note></span></div>
<div class=row><label><input type=checkbox id=ota_auto> Auto-check GitHub (needs STA uplink below)</label>
<label>Every <input id=ota_int_h size=4 title="hours, 0 = manual only"> h</label>
<button onclick="ota('check')">Check now</button><button id=otainstall style="display:none" class=warn onclick="ota('install')">Install update</button><a href=/update>Firmware upload</a><span class=msg id=otamsg></span></div>
<div class=row><label>Custom firmware URL <input id=ota_url size=28 placeholder="blank = GitHub releases"></label>
<button onclick="saveOtaUrl()">Save URL</button><button class=warn onclick="ota('url_upgrade')">Upgrade from URL</button><span class=msg id=otaurlmsg></span></div>
<div class=row><label><input type=checkbox id=sta_en> STA uplink (hotspot for internet)</label>
<label>SSID <input id=sta_ssid size=12></label><label>Pass <input id=sta_pass type=password size=12 placeholder="blank = keep"></label>
<button onclick="staTest()">Test uplink</button><button onclick="saveAdmin()">Save</button><span class=note id=stamsg></span></div>
<div class=row><span class=note id=sta_test_msg></span></div>
<div class=row><a href=/api/backup>Backup configuration</a><span class=note> (download JSON — save before upgrading; passwords never included)</span></div>
<div class=row><label>Restore backup file <input type=file id=restorefile accept=.json></label><button onclick="restore()">Restore</button><span class=msg id=restoremsg></span></div></div>
<div class=card><h3>Admin &amp; Wi-Fi AP</h3>
<div class=row><label>SSID <input id=ap_ssid size=14></label><label>Pass (8+, blank = keep) <input id=ap_pass type=password size=14 placeholder="blank = keep"></label><label>Ch <input id=ap_ch size=3></label></div>
<div class=row><label>New admin pass (4+, blank = keep) <input id=a_pass type=password size=14 placeholder="blank = keep"></label></div>
<div class=row><span class=note>Reboot, factory reset, update upload and these saves ask for the admin password (default admin123).</span></div>
<div class=row><label><input type=checkbox id=auto> Auto-start sequence on boot (burn-in)</label></div>
<div class=row><button onclick="saveAdmin()">Save</button><button class=warn onclick="saveReboot()">Save + reboot</button>
<button class=warn onclick="if(confirm('Reboot?'))admin('reboot')">Reboot</button>
<button class=danger onclick="if(confirm('Factory reset?'))admin('reset')">Factory reset</button><span class=msg id=admmsg></span></div>
<div class=row><button onclick="if(confirm('Reset settings but keep Wi-Fi + passwords?'))admin('reset_keepwifi')">Reset settings (keep Wi-Fi)</button>
<button onclick="admin('bootcount_reset')">Reset boot counter</button></div></div>
<div class=card><h3>Information</h3>
<div class=row><span class=note id=info_fw></span></div>
<div class=row><span class=note id=info_mem></span></div>
<div class=row><span class=note id=info_net></span></div>
<div class=row><span class=note>Relays R1-R8: GPIO 5,6,7,8,9,12,13,14 (active-LOW) · UART2 TX17/RX16 DE4 · Button 15 · Kill 18 · Spoof pin on Fault card · Safe spare GPIO: 1,2,21,38-44,47</span></div></div>
<div class=card><h3>Console</h3>
<div class=row><input id=cmd size=30 placeholder="HELP"><button onclick="cmd()">Run</button></div>
<div class=row><span class=note id=cmdout></span></div>
<div class=row><span class=note>START STOP FIRE CANCEL STATUS UPTIME VERSION REBOOT RESET HELP — hardware verbs ask for the admin password.</span></div></div>
</div>
<script>
async function jget(u){let r=await fetch(u);return r.json();}
async function jpost(u,b){let r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});return r.json();}
let NREL=8;
let dirty={};  // fields the user edited since last save: fillForm must not
function markDirty(e){if(e&&e.target&&e.target.id)dirty[e.target.id]=1;}
function clean(keys){for(let k of keys)delete dirty[k];}
document.addEventListener('input',markDirty);
document.addEventListener('change',markDirty);
async function refresh(){let s;try{s=await jget('/api/state');}catch(e){return null;}
 document.getElementById('fwver').textContent=s.fw||'';
 let lk=document.getElementById('link');lk.textContent=s.link?'LINK GREEN':'LINK RED';lk.className='pill '+(s.link?'G':'R');
 document.getElementById('clk').textContent='seq '+(s.running?'RUNNING':'IDLE')+' · cyc '+s.cycles+' · act '+s.acts;
 document.getElementById('spoofmsg').textContent=s.spoof?('SPOOF STAGE '+s.stage):'';
 NREL=s.cfg.nrel||8;
 let d=document.getElementById('relays');d.innerHTML='';
 s.relays.forEach((on,i)=>{let lim=i>=NREL;let nm=(s.cfg['lbl'+i]||('R'+(i+1)));d.innerHTML+=`<button class="rly${on?' on':''}${lim?' lim':''}" ${lim?'disabled':''} onclick="relay(${i},${on?0:1})">${nm}<small>${on?'ON':'OFF'}</small></button>`;});
 document.getElementById('ota_status').textContent='OTA: '+(s.cfg.ota_status||'');
 document.getElementById('ota_latest').textContent=s.cfg.ota_pending?('update available: '+s.cfg.ota_latest):('latest: '+(s.cfg.ota_latest||'?'));
 document.getElementById('otainstall').style.display=s.cfg.ota_pending?'':'none';
  document.getElementById('stamsg').textContent=s.cfg.sta===2?'STA online':(s.cfg.sta===1?'STA connecting':'STA off');
  document.getElementById('sta_test_msg').textContent='uplink test: '+(s.cfg.sta_test_msg||'not tested');
  document.getElementById('info_fw').textContent='fw '+s.fw+' ('+(s.cfg.variant||'?')+') · flash '+(s.cfg.flash_kb||'?')+' KB · free sketch '+(s.cfg.sketch_free||'?')+' B · boot #'+(s.cfg.boot||'?')+' · reset: '+(s.cfg.reset||'?');
  document.getElementById('info_mem').textContent='heap '+(s.cfg.heap||'?')+' B · psram '+(s.cfg.psram||'0')+' B · up '+(s.cfg.uptime_s||'0')+' s';
  document.getElementById('info_net').textContent='STA rssi '+(s.cfg.rssi||'0')+' dBm · STA ip '+(s.cfg.sta_ip||'-')+' · MAC '+(s.cfg.sta_mac||'-');
  return s;
}
// v2.4 per-mode menu: only the active mode's fields are shown (R1/R3).
// Hidden fields keep their saved values (fillForm still fills them).
function showMode(){let m=+document.getElementById('rmode').value;
  document.getElementById('row_step').style.display=(m===1)?'none':'';
  document.getElementById('row_seqonly').style.display=(m===0)?'':'none';
  document.getElementById('row_chaseonly').style.display=(m===2)?'':'none';
  document.getElementById('row_allonly').style.display=(m===1)?'':'none';
  document.getElementById('row_dir').style.display=(m===1)?'none':'';
}
function fillForm(s){if(!s||!s.cfg)return;
 let lb=document.getElementById('labels');if(lb&&lb.children.length===0){let h='';for(let i=0;i<8;i++)h+=`<label>R${i+1} <input id="lbl${i}" size=8></label>`;lb.innerHTML=h;}
  for(let k of ['rmode','nrel','step','hseq','swp','hall','bmode','alow','dir','loop','cpause','clim','stag','auto','sena','sv','sa','sc','ssoc','ssec','s2v','s2a','s2c','s2soc','s2sec','spin','ota_auto','ota_int_h','ota_url','sta_en','sta_ssid','ap_ssid','ap_ch','lbl0','lbl1','lbl2','lbl3','lbl4','lbl5','lbl6','lbl7']){let e=document.getElementById(k);if(e&&!dirty[k]&&document.activeElement!==e){if(e.type==='checkbox')e.checked=!!s.cfg[k];else e.value=(s.cfg[k]===undefined?'':s.cfg[k]);}}
  showMode();
}
async function loadForm(){let s;try{s=await jget('/api/state');}catch(e){return;}fillForm(s);}
async function relay(i,on){if(i>=NREL)return;await jpost('/api/relay',{i,on});refresh();}
async function seq(c){let r=await jpost('/api/seq',{cmd:c});if(!r.ok)alert(r.err||'ERR');refresh();}
async function saveCfg(){let ks=['rmode','nrel','step','hseq','swp','hall','bmode','alow','dir','cpause','clim','stag'];let b={};for(let k of ks){b[k]=+document.getElementById(k).value;}b.loop=document.getElementById('loop').checked?1:0;let r=await jpost('/api/config',b);document.getElementById('cfgmsg').textContent=r.ok?'saved':'ERR: '+(r.err||'');if(r.ok){clean(ks);clean(['loop']);loadForm();}refresh();}
async function saveLabels(){let ks=[];for(let i=0;i<8;i++)ks.push('lbl'+i);let b={};for(let k of ks)b[k]=document.getElementById(k).value;let r=await jpost('/api/config',b);document.getElementById('lblmsg').textContent=r.ok?'saved':'ERR';if(r.ok){clean(ks);loadForm();}refresh();}
async function spoof(c){let ks=['sv','sa','sc','ssoc','ssec','s2v','s2a','s2c','s2soc','s2sec','spin'];let b={cmd:c};if(c==='fire'||c==='save'){for(let k of ks)b[k]=+document.getElementById(k).value;b.sena=document.getElementById('sena').checked?1:0;}let r=await jpost('/api/spoof',b);let m=r.ok?(c==='cancel'?'cancelled':(c==='save'?'saved':'ok')):('ERR: '+(r.err||''));document.getElementById('spoofmsg').textContent=m;document.getElementById('spoofsave').textContent=m;if(r.ok&&(c==='fire'||c==='save')){clean(ks);clean(['sena']);loadForm();}refresh();}
async function staTest(){let p=prompt('Admin password:','');if(p===null)return;let b={cmd:'test',pass:p};b.ssid=document.getElementById('sta_ssid').value;b.sta_pass=document.getElementById('sta_pass').value;let r=await jpost('/api/sta',b);document.getElementById('sta_test_msg').textContent=r.ok?'testing… (watch this line)':'ERR: '+(r.err||'');refresh();}
async function saveOtaUrl(){let p=prompt('Admin password:','');if(p===null)return;let r=await jpost('/api/ota',{ota_url:document.getElementById('ota_url').value,ota_int_h:+document.getElementById('ota_int_h').value,pass:p});document.getElementById('otaurlmsg').textContent=r.ok?'saved':'ERR: '+(r.err||'');if(r.ok){clean(['ota_url','ota_int_h']);loadForm();}refresh();}
async function cmd(){let c=document.getElementById('cmd').value;let priv=/^(start|stop|fire|cancel|reboot|reset)\b/i.test(c);let b={cmd:c};if(priv){let p=prompt('Admin password:','');if(p===null)return;b.pass=p;}let r=await jpost('/api/cmd',b);document.getElementById('cmdout').textContent=r.ok?(r.out||'ok'):'ERR: '+(r.err||'');refresh();}
async function restore(){let f=document.getElementById('restorefile').files[0];if(!f){document.getElementById('restoremsg').textContent='pick a backup file first';return;}let t=await f.text();let p=prompt('Admin password:','');if(p===null)return;let b;try{b=JSON.parse(t);}catch(e){document.getElementById('restoremsg').textContent='not a JSON backup';return;}b.pass=p;let r=await jpost('/api/restore',b);document.getElementById('restoremsg').textContent=r.ok?('restored — '+(r.note||'')):'ERR: '+(r.err||'');if(r.ok)loadForm();refresh();}
async function ota(c){let p=prompt('Admin password:','');if(p===null)return;document.getElementById('otamsg').textContent=(c==='install'?'installing — box reboots on success':'checking…');let r=await jpost('/api/ota',{cmd:c,pass:p});document.getElementById('otamsg').textContent=r.ok?(c==='install'?'install started':'started'):'ERR: '+(r.err||'');refresh();}
async function saveAdmin(){let ks=['ap_ssid','ap_pass','ap_ch','a_pass','sta_ssid','sta_pass'];let b={};for(let k of ks)b[k]=document.getElementById(k).value;b.sta_en=document.getElementById('sta_en').checked?1:0;b.auto=document.getElementById('auto').checked?1:0;let p=prompt('Admin password:','');if(p===null)return;b.pass=p;let bb={pass:p};for(let k of ['ota_auto'])bb[k]=document.getElementById(k).checked?1:0;await jpost('/api/ota',bb);let r=await jpost('/api/admin',b);document.getElementById('admmsg').textContent=r.ok?'saved (reboot to apply AP/STA)':'ERR: '+r.err;if(r.ok){clean(ks);clean(['sta_en','auto','ota_auto']);document.getElementById('ap_pass').value='';document.getElementById('a_pass').value='';document.getElementById('sta_pass').value='';loadForm();}refresh();}
async function admin(c){let p=prompt('Admin password:','');if(p===null)return;let r=await jpost('/api/admin',{cmd:c,pass:p});if(!r.ok)alert('ERR: '+(r.err||''));else if(c==='reset_keepwifi'||c==='bootcount_reset')refresh();}
async function saveReboot(){await saveAdmin();if(!confirm('Saved. Reboot now to apply AP/STA?'))return;let p=prompt('Admin password (reboot):','');if(p===null)return;await jpost('/api/admin',{cmd:'reboot',pass:p});}
setInterval(refresh,1000);refresh();loadForm();
</script></body></html>)HTML";

// ---- route helpers ----
static void send_json(const String &s) {
  server.send(200, "application/json", s);
}

static void handle_root() {
  server.send_P(200, "text/html", PAGE_DASH);
}

// v2.4 STA uplink test state lives just above handle_state (it is surfaced
// there); the tick + endpoint follow the WiFi section further down.
static int sta_test = 0;  // 0 idle/never, 1 running, 2 ok, 3 fail
static char sta_test_msg[96] = "not tested";
static unsigned long sta_test_t0 = 0;
#define STA_TEST_MS 30000UL

static void handle_state() {
  Bms2Config &c = *G->cfg;
  uint8_t stage = G->spoof->stage(millis());
  String s = "{\"fw\":\"" + String(FW_VERSION) + "\"";
  s += ",\"link\":";
  s += (*G->link_green ? "true" : "false");
  s += ",\"running\":";
  s += (G->seq->running() ? "true" : "false");
  s += ",\"spoof\":";
  s += (stage != 0 ? "true" : "false");
  s += ",\"stage\":";
  s += String(stage);
  s += ",\"cycles\":";
  s += String(G->seq->cyclesDone());
  s += ",\"acts\":";
  s += String(G->seq->actuations());
  s += ",\"relays\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (i) s += ",";
    s += (G->seq->relayOn(i) ? "1" : "0");
  }
  s += "],\"cfg\":{\"rmode\":" + String(c.relay_mode) +
       ",\"nrel\":" + String(c.relay_count) +
       ",\"step\":" + String(c.step_delay_ms) +
       ",\"hseq\":" + String(c.hold_seq_ms) +
       ",\"swp\":" + String(c.chase_sweeps) +
       ",\"hall\":" + String(c.hold_all_ms) +
       ",\"bmode\":" + String((int)c.button_mode) +
       ",\"alow\":" + String(c.active_low ? 1 : 0) +
       ",\"loop\":" + String(c.loop_enabled ? 1 : 0) +
       ",\"cpause\":" + String(c.cycle_pause_ms) +
       ",\"clim\":" + String(c.cycle_limit) +
       ",\"stag\":" + String(c.allon_stagger_ms) +
       ",\"dir\":" + String(c.seq_dir) +
       ",\"auto\":" + String(c.boot_autostart ? 1 : 0) +
       ",\"sena\":" + String(c.spoof_enabled ? 1 : 0) +
       ",\"spin\":" + String(c.spoof_pin) +
       ",\"sv\":" + String(c.spoof_v_tenth) +
       ",\"sa\":" + String(c.spoof_a_tenth) +
       ",\"sc\":" + String(c.spoof_c_tenth) +
       ",\"ssoc\":" + String(c.spoof_soc) +
       ",\"ssec\":" + String(c.spoof_seconds) +
       ",\"s2v\":" + String(c.s2_v_tenth) +
       ",\"s2a\":" + String(c.s2_a_tenth) +
       ",\"s2c\":" + String(c.s2_c_tenth) +
       ",\"s2soc\":" + String(c.s2_soc) +
       ",\"s2sec\":" + String(c.s2_seconds) +
       ",\"sta\":" + String(web_sta_state()) +
       ",\"sta_en\":" + String(ident.sta_en ? 1 : 0) +
       ",\"sta_ssid\":\"" + String(ident.sta_ssid) + "\"";
  if (G->ota) {
    s += ",\"ota_auto\":" + String(G->ota->auto_enabled ? 1 : 0) +
         ",\"ota_latest\":\"" + String(G->ota->latest_tag) + "\"" +
         ",\"ota_pending\":" + String(G->ota->update_pending ? 1 : 0) +
         ",\"ota_status\":\"" + String(G->ota->status) + "\"";
    char num[24];
    snprintf(num, sizeof(num), "%lu",
             (unsigned long)(G->ota->interval_ms / 3600UL / 1000UL));
    s += ",\"ota_int_h\":"; s += num;
  }
  // v2.4 Information card (Tasmota Status 1/2/4/5/7, minus MQTT/sensors).
  {
    char num[32];
    s += ",\"variant\":\""; s += FW_VARIANT; s += "\"";
    snprintf(num, sizeof(num), "%lu",
             (unsigned long)(ESP.getFlashChipSize() / 1024u));
    s += ",\"flash_kb\":"; s += num;
    snprintf(num, sizeof(num), "%lu",
             (unsigned long)ESP.getFreeSketchSpace());
    s += ",\"sketch_free\":"; s += num;
    snprintf(num, sizeof(num), "%lu", (unsigned long)ESP.getFreeHeap());
    s += ",\"heap\":"; s += num;
    snprintf(num, sizeof(num), "%lu", (unsigned long)ESP.getPsramSize());
    s += ",\"psram\":"; s += num;
    snprintf(num, sizeof(num), "%lu", millis() / 1000u);
    s += ",\"uptime_s\":"; s += num;
    snprintf(num, sizeof(num), "%u", web_boot_count);
    s += ",\"boot\":"; s += num;
    s += ",\"reset\":\""; s += String(web_reset_reason); s += "\"";
    snprintf(num, sizeof(num), "%ld",
             (long)(web_sta_state() == 2 ? WiFi.RSSI() : 0));
    s += ",\"rssi\":"; s += num;
    s += ",\"sta_ip\":\"";
    s += (web_sta_state() == 2 ? WiFi.localIP().toString() : String(""));
    s += "\"";
    s += ",\"sta_mac\":\""; s += WiFi.macAddress(); s += "\"";
    snprintf(num, sizeof(num), "%d", sta_test);
    s += ",\"sta_test\":"; s += num;
    s += ",\"sta_test_msg\":\""; s += String(sta_test_msg); s += "\"";
    s += ",\"ota_url\":\""; s += String(ident.ota_url); s += "\"";
  }
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    s += ",\"lbl";
    s += String(i);
    s += "\":\"";
    s += String(c.relay_label[i]);
    s += "\"";
  }
  s += ",\"ap_ssid\":\"" + String(ident.ap_ssid) +
       "\",\"ap_ch\":" + String(ident.ap_channel) + "}}";
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
  String b = server.arg("plain");
  int i = (int)jnum(b, "i", -1);
  uint8_t n = G->cfg->relay_count;
  if (n < 1 || n > RELAY_COUNT) n = RELAY_COUNT;
  if (i < 0 || i >= n) { server.send(400); return; }  // beyond count: OFF zone
  G->seq->setForced((uint8_t)i, jnum(b, "on", 0) != 0);
  send_json("{\"ok\":1}");
}

static void handle_seq() {
  String b = server.arg("plain");
  if (jstr(b, "cmd") == "start") {
    // v2.4: start() can refuse inside the post-stop dead-band (R12).
    if (!G->seq->start(millis())) {
      send_json("{\"ok\":0,\"err\":\"relays settling — wait a beat, then START\"}");
      return;
    }
  } else {
    G->seq->stopAll(millis());
  }
  send_json("{\"ok\":1}");
}

// Shared config application (sequence card + labels). Validates onto a
// scratch copy first: a rejected save changes NOTHING, and both the live
// /api/config path and backup-restore share one clamp table. Returns false
// with err set on R9/R10 rejects.
static bool apply_cfg_keys(const String &b, String &err) {
  Bms2Config &c = *G->cfg;
  // Validate-then-commit: apply the POST onto a scratch copy so a rejected
  // save changes NOTHING (bench stays in the last known-good state).
  Bms2Config t = c;
  if (has(b, "rmode")) {
    long m = jnum(b, "rmode", 0);
    t.relay_mode = (m < 0 || m > 2) ? RELAY_SEQUENTIAL : (uint8_t)m;
  }
  if (has(b, "nrel"))
    t.relay_count = (uint8_t)constrain(jnum(b, "nrel", 8), 1, RELAY_COUNT);
  // v2.4 floors (R4/R5/R6): step >= 100 ms (bounce + ADC settle);
  // stagger 0 = explicit at-once slam (allowed, warned), else 20..1000;
  // pause 500..60000 (coil cooling; immediate re-trigger wears contacts).
  if (has(b, "step")) t.step_delay_ms = (uint16_t)constrain(jnum(b, "step", 250), 100, 60000);
  if (has(b, "hseq")) t.hold_seq_ms = (uint32_t)constrain(jnum(b, "hseq", 30000), 0, 3600000);
  if (has(b, "swp")) t.chase_sweeps = (uint8_t)constrain(jnum(b, "swp", 3), 0, 100);
  if (has(b, "hall")) t.hold_all_ms = (uint32_t)constrain(jnum(b, "hall", 300000), 0, 3600000);
  if (has(b, "bmode")) {
    long m = jnum(b, "bmode", 0);
    t.button_mode = (m < 0 || m > 2) ? BTN_HOLD_ABORT : (uint8_t)m;
  }
  if (has(b, "alow")) t.active_low = jnum(b, "alow", 1) != 0;
  if (has(b, "loop")) t.loop_enabled = jnum(b, "loop", 0) != 0;
  if (has(b, "cpause"))
    t.cycle_pause_ms = (uint32_t)constrain(jnum(b, "cpause", 2000), 500, 60000);
  if (has(b, "clim"))
    t.cycle_limit = (uint16_t)constrain(jnum(b, "clim", 0), 0, 60000);
  if (has(b, "stag"))
    t.allon_stagger_ms = (uint16_t)constrain(jnum(b, "stag", 50), 0, 1000);
  if (has(b, "dir")) t.seq_dir = (jnum(b, "dir", 0) == 1) ? 1 : 0;
  if (has(b, "auto")) t.boot_autostart = jnum(b, "auto", 0) != 0;
  // R10: a mode switch strands the old mode's relays — require IDLE first.
  if (t.relay_mode != c.relay_mode && G->seq->running()) {
    err = "stop the sequence first, then switch mode";
    return false;
  }
  // R9: hold 0 never expires, so loop+limit could never complete — refuse.
  if (t.loop_enabled) {
    uint32_t h = (t.relay_mode == RELAY_ALL_ON) ? t.hold_all_ms
                 : (t.relay_mode == RELAY_CHASE)
                       ? (t.chase_sweeps == 0 ? 0 : 1)
                       : t.hold_seq_ms;
    if (h == 0) {
      err = "loop needs a finite hold (hold 0 = forever)";
      return false;
    }
  }
  bool count_grew_down = (t.relay_count != c.relay_count);
  c = t;
  if (count_grew_down) G->seq->countChanged();  // R8: drop stale forces
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    char k[8];
    snprintf(k, sizeof(k), "lbl%u", i);
    if (has(b, k)) {
      String v = jstr(b, k);
      // Labels are display-only: cap length, forbid quotes (JSON/HTML).
      if (v.length() > 0 && v.length() < (int)sizeof(c.relay_label[i])) {
        bool clean = true;
        for (unsigned ci = 0; ci < v.length(); ci++) {
          char ch = v.c_str()[ci];
          if (ch == '"' || ch == '\\' || ch < 32 || ch > 126) { clean = false; break; }
        }
        if (clean) v.toCharArray(c.relay_label[i], sizeof(c.relay_label[i]));
      }
    }
  }
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  return true;
}

static void handle_config() {
  String b = server.arg("plain");
  String err;
  if (!apply_cfg_keys(b, err)) {
    send_json(String("{\"ok\":0,\"err\":\"") + err + "\"}");
    return;
  }
  send_json("{\"ok\":1}");
}

// Shared spoof application (values + pin + enable, no trigger). Used by
// /api/spoof (save/fire) and backup-restore.
static void apply_spoof_keys(const String &b) {
  Bms2Config &c = *G->cfg;
  // Optional inline values for BOTH stages (x0.1 display units).
  if (has(b, "sv")) c.spoof_v_tenth = (uint16_t)constrain(jnum(b, "sv", 1000), 0, 9999);
  if (has(b, "sa")) c.spoof_a_tenth = (uint16_t)constrain(jnum(b, "sa", 1000), 0, 9999);
  if (has(b, "sc")) c.spoof_c_tenth = (uint16_t)constrain(jnum(b, "sc", 1000), 0, 9999);
  if (has(b, "ssoc")) c.spoof_soc = (uint8_t)constrain(jnum(b, "ssoc", 100), 0, 255);
  if (has(b, "ssec")) c.spoof_seconds = (uint16_t)constrain(jnum(b, "ssec", 5), 1, 120);
  if (has(b, "s2v")) c.s2_v_tenth = (uint16_t)constrain(jnum(b, "s2v", 888), 0, 9999);
  if (has(b, "s2a")) c.s2_a_tenth = (uint16_t)constrain(jnum(b, "s2a", 888), 0, 9999);
  if (has(b, "s2c")) c.s2_c_tenth = (uint16_t)constrain(jnum(b, "s2c", 888), 0, 9999);
  if (has(b, "s2soc")) c.s2_soc = (uint8_t)constrain(jnum(b, "s2soc", 188), 0, 255);
  if (has(b, "s2sec")) c.s2_seconds = (uint16_t)constrain(jnum(b, "s2sec", 10), 1, 120);
  if (has(b, "sena")) c.spoof_enabled = jnum(b, "sena", 1) != 0;
  // v2.3.1 configurable trigger GPIO (unsafe pins fall back to 21).
  if (has(b, "spin"))
    c.spoof_pin = sanitize_spoof_pin((uint8_t)jnum(b, "spin", 21));
}

static void handle_spoof() {
  Bms2Config &c = *G->cfg;
  String b = server.arg("plain");
  String cmd = jstr(b, "cmd");
  if (cmd == "cancel") { G->spoof->cancel(); send_json("{\"ok\":1}"); return; }
  // v2.4: explicit Save persists values+pin+enable WITHOUT firing (the old
  // FIRE-always-saved left no way to stage values). FIRE = save + trigger.
  bool want_fire = (cmd == "fire");
  if (cmd != "save" && !want_fire && cmd.length() != 0) {
    send_json("{\"ok\":0,\"err\":\"unknown spoof cmd\"}");
    return;
  }
  apply_spoof_keys(b);
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  if (want_fire)
    G->spoof->trigger(millis(), (unsigned long)c.spoof_seconds * 1000UL,
                      (unsigned long)c.s2_seconds * 1000UL);
  send_json("{\"ok\":1}");
}

static void handle_admin() {
  String b = server.arg("plain");
  if (!admin_ok(jstr(b, "pass"))) { send_json("{\"ok\":0,\"err\":\"admin password required\"}"); return; }
  String cmd = jstr(b, "cmd");
  if (cmd == "reset") {
    send_json("{\"ok\":1}");
    web_factory_reset();
    return;
  }
  // v2.4 Reset-4 equivalent: wipe bench settings but KEEP AP/STA/admin
  // identity (the old all-or-nothing reset is why people feared the button).
  if (cmd == "reset_keepwifi") {
    WebIdentity keep = ident;
    bool keep_ota_auto = G->ota ? G->ota->auto_enabled : false;
    unsigned long keep_ota_int = G->ota ? G->ota->interval_ms : 0;
    cfg_dirty = false;
    nvs.begin(WEB_NVS_NS, false);
    nvs.clear();
    nvs.end();
    ident = keep;
    if (G->ota) {
      G->ota->auto_enabled = keep_ota_auto;
      G->ota->interval_ms = keep_ota_int;
    }
    *G->cfg = Bms2Config();  // compiled defaults back in RAM
    if (G->on_config_changed) G->on_config_changed();
    cfg_save();
    send_json("{\"ok\":1}");
    web_reboot_now();
    return;
  }
  if (cmd == "bootcount_reset") {  // Tasmota Reset-99, no reboot needed
    web_bootcount_reset();
    send_json("{\"ok\":1}");
    return;
  }
  if (cmd == "reboot") {
    send_json("{\"ok\":1}");
    web_reboot_now();  // flushes pending saves first: reboot wipes RAM
    return;
  }
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
  if (has(b, "a_pass")) {
    String v = jstr(b, "a_pass");
    if (v.length() >= 4 && v.length() < (int)sizeof(ident.admin_pass))
      v.toCharArray(ident.admin_pass, sizeof(ident.admin_pass));
    else if (v.length() > 0) {
      send_json("{\"ok\":0,\"err\":\"admin password needs 4+ chars\"}");
      return;
    }
  }
  // v2.3 optional STA uplink (for GitHub OTA checks only; AP stays always on).
  if (has(b, "sta_en")) ident.sta_en = jnum(b, "sta_en", 0) != 0;
  if (has(b, "sta_ssid")) {
    String v = jstr(b, "sta_ssid");
    if (v.length() < (int)sizeof(ident.sta_ssid))
      v.toCharArray(ident.sta_ssid, sizeof(ident.sta_ssid));
  }
  if (has(b, "sta_pass")) {
    String v = jstr(b, "sta_pass");
    if (v.length() > 0 && v.length() < (int)sizeof(ident.sta_pass))
      v.toCharArray(ident.sta_pass, sizeof(ident.sta_pass));
  }
  // v2.3 boot auto-start lives on the Admin card (applies after reboot).
  if (has(b, "auto")) G->cfg->boot_autostart = jnum(b, "auto", 0) != 0;
  cfg_save();
  send_json("{\"ok\":1}");
}

// v2.3 OTA control: auto toggle + manual "check now" (main.cpp does network).
// Gated like /api/admin: checking/installing can flash firmware.
static void handle_ota() {
  String b = server.arg("plain");
  if (!admin_ok(jstr(b, "pass"))) { send_json("{\"ok\":0,\"err\":\"admin password required\"}"); return; }
  if (has(b, "ota_auto") && G->ota) {
    G->ota->auto_enabled = jnum(b, "ota_auto", 0) != 0;
    cfg_save();
  }
  // v2.4: check cadence editable, hours (0 = manual only).
  if (has(b, "ota_int_h") && G->ota) {
    long h = jnum(b, "ota_int_h", 24);
    if (h < 0) h = 0;
    if (h > 24 * 30) h = 24 * 30;
    G->ota->interval_ms = (unsigned long)h * 3600UL * 1000UL;
    cfg_save();
  }
  // v2.4 custom firmware URL (Tasmota OtaUrl): http(s) .bin only, NVS-kept.
  if (has(b, "ota_url")) {
    String v = jstr(b, "ota_url");
    if (v.length() == 0) {
      ident.ota_url[0] = '\0';  // blank = back to GitHub releases
      cfg_save();
    } else if (v.length() < (int)sizeof(ident.ota_url) &&
               (v.indexOf("http://") == 0 || v.indexOf("https://") == 0) &&
               v.indexOf(".bin") > 0) {
      v.toCharArray(ident.ota_url, sizeof(ident.ota_url));
      cfg_save();
    } else {
      send_json("{\"ok\":0,\"err\":\"ota url must be http(s) .bin\"}");
      return;
    }
  }
  if (jstr(b, "cmd") == "check") {
    if (web_sta_state() != 2) {
      send_json("{\"ok\":0,\"err\":\"STA offline (set uplink + reboot)\"}");
      return;
    }
    if (G->on_ota_check) G->on_ota_check();
    send_json("{\"ok\":1}");
    return;
  }
  if (jstr(b, "cmd") == "install") {
    if (!G->ota || !G->ota->update_pending ||
        G->ota->latest_tag[0] == '\0') {
      send_json("{\"ok\":0,\"err\":\"no update pending (check first)\"}");
      return;
    }
    if (web_sta_state() != 2) {
      send_json("{\"ok\":0,\"err\":\"STA offline (set uplink + reboot)\"}");
      return;
    }
    if (G->on_ota_install) G->on_ota_install();  // reboots on success
    send_json("{\"ok\":1}");
    return;
  }
  // v2.4 Upgrade-from-URL (Tasmota u1 box): needs STA online; the install
  // path enforces the same magic/size/variant gates as file upload.
  if (jstr(b, "cmd") == "url_upgrade") {
    if (ident.ota_url[0] == '\0') {
      send_json("{\"ok\":0,\"err\":\"set a custom firmware URL first\"}");
      return;
    }
    if (web_sta_state() != 2) {
      send_json("{\"ok\":0,\"err\":\"STA offline (test the uplink first)\"}");
      return;
    }
    if (G->on_ota_install) G->on_ota_install();  // reboots on success
    send_json("{\"ok\":1}");
    return;
  }
  send_json("{\"ok\":1}");
}

const char *web_ota_url() { return ident.ota_url; }

// ---- v2.3 manual firmware upload (works fully offline) ----
static const char PAGE_UPDATE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>BMS Tester firmware update</title><style>body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#e2e8f0}form{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:24px}input[type=submit]{background:#b45309;border:0;color:#fff;border-radius:8px;padding:10px 16px;font-size:15px;cursor:pointer}a{color:#60a5fa}#bar{height:10px;background:#0f172a;border-radius:6px;margin-top:12px}#fill{height:10px;width:0;background:#b45309;border-radius:6px}</style></head><body style="max-width:480px;margin:60px auto;padding:12px">
<h2>Firmware update</h2>
<p>Upload the <b>.bin</b> from the GitHub release matching this box.
The box checks the file (right variant, real firmware image, fits flash)
BEFORE writing, shows progress, and reboots on success. A rejected file or a
wrong password changes nothing — the box keeps running.</p>
<form method=POST action=/update enctype=multipart/form-data>
Admin password<br><input name=pass type=password><br><br>
<input type=file name=firmware accept=.bin><br><br>
<input type=submit value="Upload + reboot" onclick="poll()">
</form><div id=bar><div id=fill></div></div><p id=msg></p><p><a href=/>Back to dashboard</a></p>
<script>async function poll(){let m=document.getElementById('msg'),f=document.getElementById('fill');m.textContent='uploading…';let t=setInterval(async()=>{try{let r=await fetch('/api/uprog');let j=await r.json();if(j.max>0)f.style.width=Math.min(100,100*j.written/j.max)+'%';if(j.err!=='ok'){m.textContent='stopped: '+j.err;clearInterval(t);}}catch(e){}},500);setTimeout(()=>clearInterval(t),120000);}</script></body></html>)HTML";

static void handle_update_page() {
  server.send_P(200, "text/html", PAGE_UPDATE);
}

// ---- v2.4 Tasmota-grade manual firmware upload (works fully offline) ----
// Tasmota rules adopted: explicit sketch budget (never SIZE_UNKNOWN), exact
// variant-asset filename match, 0xE9 magic + flash-size-vs-chip gate on the
// first 4 bytes, ONE Update.end() in the done handler only, progress readout,
// and every failure names its cause. Staging without end(true) never boots,
// so a wrong password or a bad file aborts with zero effect on the box.
static String up_pass = "";
static FwUploadErr up_err = FW_OK;
static bool up_begun = false;
static uint8_t up_head[4];
static size_t up_head_n = 0;
static uint32_t up_max_space = 0;
static size_t up_written = 0;

static void up_reset() {
  up_pass = "";
  up_err = FW_OK;
  up_begun = false;
  up_head_n = 0;
  up_max_space = 0;
  up_written = 0;
}

static void handle_update_upload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (up.filename.length() == 0) return;  // form-field part, not the file
    // Fresh file part: reset FILE state but PRESERVE up_pass — the pass
    // field may stream before the file part (browser form order) or after
    // it; both orders must work. Full reset happens in the done handler.
    up_err = FW_OK;
    up_begun = false;
    up_head_n = 0;
    up_written = 0;
    up_max_space = 0;
    // Gate 1: exact variant asset (no cross-flashing 8mb <-> n16r8).
    if (!fw_filename_ok(up.filename.c_str(), FW_VARIANT)) {
      up_err = FW_WRONG_FILE;
      return;
    }
    // Gate 2: explicit budget (Tasmota's maxSketchSpace, not UNKNOWN).
    up_max_space = fw_max_sketch_space((uint32_t)ESP.getFreeSketchSpace());
    if (up_max_space == 0) {
      up_err = FW_BEGIN_FAIL;
      return;
    }
    // Update.begin is deferred to the first gated WRITE (after the head
    // gate passes) so rejected files never touch the OTA partition.
    return;
  }
  if (up.status == UPLOAD_FILE_WRITE) {
    if (up.filename.length() == 0) {
      // Order-independent password: collected from ANY chunk, judged once
      // in the done handler (the old code trusted form part ordering).
      if (up.name == "pass" && up_pass.length() < 64) {
        char ch[2] = {'\0', '\0'};
        for (size_t i = 0; i < up.currentSize; i++) {
          ch[0] = (char)up.buf[i];
          up_pass += ch;
        }
      }
      return;
    }
    if (up_err != FW_OK) return;  // swallow: commit nothing
    // Gate 3: image head (magic + flash size) across chunk boundaries.
    const uint8_t *p = up.buf;
    size_t n = up.currentSize;
    while (up_head_n < 4 && n > 0) {
      up_head[up_head_n++] = *p++;
      n--;
    }
    if (up_head_n < 4) return;  // head not complete yet; no bytes committed
    if (!up_begun) {
      FwUploadErr g = fw_gate_image_head(
          up_head, 4, (uint32_t)ESP.getFlashChipSize());
      if (g != FW_OK) {
        up_err = g;
        return;
      }
      if (!Update.begin(up_max_space)) {
        up_err = FW_BEGIN_FAIL;
        return;
      }
      up_begun = true;
      // The head bytes are image bytes too — commit them first, in order.
      if (Update.write(up_head, 4) != 4) {
        Update.end(false);
        up_begun = false;
        up_err = FW_WRITE_FAIL;
        return;
      }
      up_written = 4;
    }
    if (n > 0) {
      // Gate 4: explicit budget policed while streaming (named TOO_BIG
      // instead of dying inside Update with a bare error). The const cast
      // is safe: Update copies the chunk synchronously, never retains it.
      if (up_written + n > up_max_space) {
        Update.end(false);
        up_begun = false;
        up_err = FW_TOO_BIG;
        return;
      }
      if (Update.write((uint8_t *)p, n) != n) {
        Update.end(false);
        up_begun = false;
        up_err = FW_WRITE_FAIL;
        return;
      }
      up_written += n;
    }
    return;
  }
  // UPLOAD_FILE_END: nothing to finalize here — the done handler is the
  // single place that finalizes the update (the old double-finalize
  // corrupted status reporting).
}

static void handle_update_done() {
  // Both the streamed field and the parsed arg must agree (defense in depth).
  bool pass_ok = admin_ok(up_pass) && admin_ok(server.arg("pass"));
  if (!pass_ok) {
    if (up_begun) Update.end(false);  // staged bytes never boot: abort
    FwUploadErr e = up_err;
    up_reset();
    server.send(403, "text/plain", fw_err_str(e == FW_OK ? FW_BAD_PASS : e));
    return;
  }
  if (up_err != FW_OK || !up_begun) {
    if (up_begun) Update.end(false);
    FwUploadErr e = up_err == FW_OK ? FW_WRITE_FAIL : up_err;
    up_reset();
    server.send(400, "text/plain", fw_err_str(e));
    return;
  }
  if (Update.hasError() || !Update.end(true)) {
    up_reset();
    server.send(500, "text/plain", "UPDATE FAILED: flash finalize error");
    return;
  }
  up_reset();
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "UPDATE OK - rebooting");
  web_reboot_now();
}

// Upload progress for the /update page (polled while the file streams).
static void handle_uprog() {
  char b[96];
  snprintf(b, sizeof(b), "{\"written\":%u,\"max\":%lu,\"err\":\"%s\"}",
           (unsigned)up_written, (unsigned long)up_max_space,
           fw_err_str(up_err));
  send_json(b);
}

// ---- v2.3 optional STA uplink (phone hotspot, for OTA checks only) ----
static int sta_state = 0;  // 0 off, 1 connecting, 2 online
static unsigned long sta_t0 = 0;
#define STA_TRY_MS 30000UL

int web_sta_state() { return sta_state; }

static void sta_begin() {
  if (!ident.sta_en || ident.sta_ssid[0] == '\0') {
    sta_state = 0;
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ident.sta_ssid, ident.sta_pass);
  sta_state = 1;
  sta_t0 = millis();
}

static void sta_tick(unsigned long now) {
  if (sta_state != 1) return;
  if (WiFi.status() == WL_CONNECTED) {
    sta_state = 2;  // online; AP keeps running alongside
    return;
  }
  if ((now - sta_t0) >= STA_TRY_MS) sta_state = 0;  // AP-only fallback
}

static void sta_test_tick(unsigned long now) {
  if (sta_test != 1) return;
  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    char b[96];
    snprintf(b, sizeof(b), "joined, RSSI %ld dBm, IP %s", (long)WiFi.RSSI(),
             ip.c_str());
    strncpy(sta_test_msg, b, sizeof(sta_test_msg) - 1);
    sta_test_msg[sizeof(sta_test_msg) - 1] = '\0';
    WiFi.disconnect(true);  // test only: back to AP-only
    WiFi.mode(WIFI_AP);
    sta_test = 2;
    return;
  }
  if ((now - sta_test_t0) >= STA_TEST_MS) {
    strncpy(sta_test_msg,
            "join timeout: wrong pass? 5 GHz-only hotspot? out of range?",
            sizeof(sta_test_msg) - 1);
    sta_test_msg[sizeof(sta_test_msg) - 1] = '\0';
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    sta_test = 3;
  }
}

static void handle_sta() {
  String b = server.arg("plain");
  if (!admin_ok(jstr(b, "pass"))) { send_json("{\"ok\":0,\"err\":\"admin password required\"}"); return; }
  if (jstr(b, "cmd") != "test") { send_json("{\"ok\":0,\"err\":\"unknown sta cmd\"}"); return; }
  // Test the given credentials if supplied, else the saved ones.
  char ssid[32], pass[64];
  if (has(b, "ssid")) {
    String v = jstr(b, "ssid");
    if (v.length() == 0 || v.length() >= (int)sizeof(ssid)) {
      send_json("{\"ok\":0,\"err\":\"no SSID to test\"}");
      return;
    }
    v.toCharArray(ssid, sizeof(ssid));
    String w = jstr(b, "sta_pass");
    w.toCharArray(pass, sizeof(pass));
  } else {
    if (ident.sta_ssid[0] == '\0') {
      send_json("{\"ok\":0,\"err\":\"no saved SSID (enter one first)\"}");
      return;
    }
    strncpy(ssid, ident.sta_ssid, sizeof(ssid));
    strncpy(pass, ident.sta_pass, sizeof(pass));
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, pass);
  sta_test = 1;
  sta_test_t0 = millis();
  strncpy(sta_test_msg, "testing...", sizeof(sta_test_msg) - 1);
  send_json("{\"ok\":1}");
}

// v2.4 web console (Tasmota Console idea, bench verbs only — no scripting).
// Read-only verbs are free; anything that moves hardware needs the admin
// password, exactly like the buttons it mirrors.
static void handle_cmd() {
  String b = server.arg("plain");
  String raw = jstr(b, "cmd");
  // First whitespace-separated token, lowercased.
  char verb[16] = "";
  {
    size_t n = 0;
    for (unsigned i = 0; i < raw.length() && n < sizeof(verb) - 1; i++) {
      char ch = raw.c_str()[i];
      if (ch == ' ' || ch == '\t') break;
      if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
      verb[n++] = ch;
    }
    verb[n] = '\0';
  }
  String out;
  bool priv = (strcmp(verb, "start") == 0 || strcmp(verb, "stop") == 0 ||
               strcmp(verb, "fire") == 0 || strcmp(verb, "cancel") == 0 ||
               strcmp(verb, "reboot") == 0 || strcmp(verb, "reset") == 0);
  if (priv && !admin_ok(jstr(b, "pass"))) {
    send_json("{\"ok\":0,\"err\":\"admin password required\"}");
    return;
  }
  if (strcmp(verb, "start") == 0) {
    if (!G->seq->start(millis())) out = "refused: 500 ms post-stop dead-band";
    else out = "started";
  } else if (strcmp(verb, "stop") == 0) {
    G->seq->stopAll(millis());
    out = "stopped";
  } else if (strcmp(verb, "fire") == 0) {
    Bms2Config &c = *G->cfg;
    G->spoof->trigger(millis(), (unsigned long)c.spoof_seconds * 1000UL,
                      (unsigned long)c.s2_seconds * 1000UL);
    out = "spoof fired";
  } else if (strcmp(verb, "cancel") == 0) {
    G->spoof->cancel();
    out = "spoof cancelled";
  } else if (strcmp(verb, "status") == 0) {
    char s[96];
    snprintf(s, sizeof(s), "%s seq=%s cyc=%lu act=%lu spoof=%u",
             *G->link_green ? "LINK GREEN" : "LINK RED",
             G->seq->running() ? "RUNNING" : "IDLE",
             G->seq->cyclesDone(), G->seq->actuations(),
             G->spoof->stage(millis()));
    out = s;
  } else if (strcmp(verb, "uptime") == 0) {
    char s[48];
    snprintf(s, sizeof(s), "up %lus boot #%u", millis() / 1000u,
             web_boot_count);
    out = s;
  } else if (strcmp(verb, "version") == 0) {
    out = String("bms-tester ") + FW_VERSION + " (" + FW_VARIANT + ")";
  } else if (strcmp(verb, "reboot") == 0) {
    send_json("{\"ok\":1,\"out\":\"rebooting\"}");
    web_reboot_now();
    return;
  } else if (strcmp(verb, "reset") == 0) {
    send_json("{\"ok\":1,\"out\":\"factory reset\"}");
    web_factory_reset();
    return;
  } else if (strcmp(verb, "help") == 0 || verb[0] == '\0') {
    out = "START STOP FIRE CANCEL STATUS UPTIME VERSION REBOOT RESET HELP";
  } else {
    send_json("{\"ok\":0,\"err\":\"unknown command (try HELP)\"}");
    return;
  }
  send_json("{\"ok\":1,\"out\":\"" + out + "\"}");
}

// v2.4 config backup (Tasmota pre-upgrade ritual): whole NVS config as JSON.
// Passwords NEVER leave the box (ap/admin/sta secrets excluded by design).
static void handle_backup() {
  Bms2Config &c = *G->cfg;
  char num[32];
  String s = "{\"backup\":1,\"fw\":\"" + String(FW_VERSION) + "\"";
  snprintf(num, sizeof(num), "%u", c.step_delay_ms);
  s += ",\"step\":"; s += num;
  snprintf(num, sizeof(num), "%lu", (unsigned long)c.hold_seq_ms);
  s += ",\"hseq\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.chase_sweeps);
  s += ",\"swp\":"; s += num;
  snprintf(num, sizeof(num), "%lu", (unsigned long)c.hold_all_ms);
  s += ",\"hall\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.relay_count);
  s += ",\"nrel\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.relay_mode);
  s += ",\"rmode\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.button_mode);
  s += ",\"bmode\":"; s += num;
  s += ",\"alow\":"; s += String(c.active_low ? 1 : 0);
  s += ",\"loop\":"; s += String(c.loop_enabled ? 1 : 0);
  snprintf(num, sizeof(num), "%lu", (unsigned long)c.cycle_pause_ms);
  s += ",\"cpause\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.cycle_limit);
  s += ",\"clim\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.allon_stagger_ms);
  s += ",\"stag\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.seq_dir);
  s += ",\"dir\":"; s += num;
  s += ",\"auto\":"; s += String(c.boot_autostart ? 1 : 0);
  s += ",\"sena\":"; s += String(c.spoof_enabled ? 1 : 0);
  snprintf(num, sizeof(num), "%u", c.spoof_pin);
  s += ",\"spin\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.spoof_v_tenth);
  s += ",\"sv\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.spoof_a_tenth);
  s += ",\"sa\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.spoof_c_tenth);
  s += ",\"sc\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.spoof_soc);
  s += ",\"ssoc\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.spoof_seconds);
  s += ",\"ssec\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.s2_v_tenth);
  s += ",\"s2v\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.s2_a_tenth);
  s += ",\"s2a\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.s2_c_tenth);
  s += ",\"s2c\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.s2_soc);
  s += ",\"s2soc\":"; s += num;
  snprintf(num, sizeof(num), "%u", c.s2_seconds);
  s += ",\"s2sec\":"; s += num;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    s += ",\"lbl"; s += String(i); s += "\":\"";
    s += String(c.relay_label[i]); s += "\"";
  }
  s += ",\"ap_ssid\":\""; s += String(ident.ap_ssid); s += "\"";
  snprintf(num, sizeof(num), "%u", ident.ap_channel);
  s += ",\"ap_ch\":"; s += num;
  s += ",\"sta_en\":"; s += String(ident.sta_en ? 1 : 0);
  s += ",\"sta_ssid\":\""; s += String(ident.sta_ssid); s += "\"";
  s += ",\"ota_auto\":"; s += String((G->ota && G->ota->auto_enabled) ? 1 : 0);
  if (G->ota) {
    snprintf(num, sizeof(num), "%lu",
             (unsigned long)(G->ota->interval_ms / 3600UL / 1000UL));
    s += ",\"ota_int_h\":"; s += num;
  }
  s += ",\"ota_url\":\""; s += String(ident.ota_url); s += "\"";
  s += "}";
  server.send(200, "application/json", s);
}

// v2.4 restore: a backup JSON (flat keys) re-applied through the SAME clamp
// table as the live endpoints. Passwords are never in backups, so restore
// never touches them — re-enter secrets by hand after a restore.
static void handle_restore() {
  String b = server.arg("plain");
  if (!admin_ok(jstr(b, "pass"))) { send_json("{\"ok\":0,\"err\":\"admin password required\"}"); return; }
  if (jnum(b, "backup", 0) != 1) {
    send_json("{\"ok\":0,\"err\":\"not a bms-tester backup file\"}");
    return;
  }
  String err;
  if (!apply_cfg_keys(b, err)) {
    send_json(String("{\"ok\":0,\"err\":\"") + err + "\"}");
    return;
  }
  apply_spoof_keys(b);
  if (has(b, "ap_ssid")) {
    String v = jstr(b, "ap_ssid");
    if (v.length() > 0 && v.length() < (int)sizeof(ident.ap_ssid))
      v.toCharArray(ident.ap_ssid, sizeof(ident.ap_ssid));
  }
  if (has(b, "ap_ch")) {
    long ch = jnum(b, "ap_ch", 6);
    if (ch >= 1 && ch <= 13) ident.ap_channel = (uint8_t)ch;
  }
  if (has(b, "sta_en")) ident.sta_en = jnum(b, "sta_en", 0) != 0;
  if (has(b, "sta_ssid")) {
    String v = jstr(b, "sta_ssid");
    if (v.length() < (int)sizeof(ident.sta_ssid))
      v.toCharArray(ident.sta_ssid, sizeof(ident.sta_ssid));
  }
  if (has(b, "ota_auto") && G->ota)
    G->ota->auto_enabled = jnum(b, "ota_auto", 0) != 0;
  if (has(b, "ota_int_h") && G->ota) {
    long h = jnum(b, "ota_int_h", 24);
    if (h < 0) h = 0;
    if (h > 24 * 30) h = 24 * 30;
    G->ota->interval_ms = (unsigned long)h * 3600UL * 1000UL;
  }
  if (has(b, "ota_url")) {
    String v = jstr(b, "ota_url");
    if (v.length() < (int)sizeof(ident.ota_url))
      v.toCharArray(ident.ota_url, sizeof(ident.ota_url));
  }
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  send_json("{\"ok\":1,\"note\":\"passwords not restored — re-enter + reboot to apply AP/STA\"}");
}

static bool wifi_is_up = false;

// AP ALWAYS ON (unless the kill switch says otherwise): no timeouts, works
// with zero office network. Fixed 192.168.4.1 gateway (documented
// everywhere) + DNS catch-all so any URL on the device resolves to us
// (captive portal).
static void wifi_up() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(ident.ap_ssid, ident.ap_pass, ident.ap_channel);
  dns.start(53, "*", WiFi.softAPIP());
  MDNS.begin("bmstester");  // v2.4: bmstester.local for laptops/desktops
  sta_begin();  // optional uplink attempt (non-blocking; AP unaffected)
  server.begin();  // re-begin is safe: routes were registered once in setup
  wifi_is_up = true;
}

// v2.3.1 WiFi kill switch: grounding the pin drops everything now;
// releasing it brings the AP back (same SSID/pass/channel).
void web_wifi_set(bool on) {
  if (on == wifi_is_up) return;
  if (!on) {
    MDNS.end();
    server.stop();
    dns.stop();
    sta_state = 0;
    WiFi.mode(WIFI_OFF);
    wifi_is_up = false;
    return;
  }
  wifi_up();
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
  ident.sta_ssid[0] = '\0';
  ident.sta_pass[0] = '\0';
  ident.sta_en = false;
  ident.ota_url[0] = '\0';
  sta_state = 0;
  cfg_dirty = false;  // fresh boot: nothing pending
  cfg_load();  // NVS -> cfg + identity (or defaults)
  web_boot_track();  // v2.4 boot counter + reset reason (info card)
  if (G->on_config_changed) G->on_config_changed();  // build spoof frames
  wifi_up();  // AP + portal + server (kill switch can drop/raise this live)
  server.on("/", handle_root);
  server.on("/update", HTTP_GET, handle_update_page);
  server.on("/update", HTTP_POST, handle_update_done, handle_update_upload);
  server.on("/api/state", HTTP_GET, handle_state);
  server.on("/api/relay", HTTP_POST, handle_relay);
  server.on("/api/seq", HTTP_POST, handle_seq);
  server.on("/api/config", HTTP_POST, handle_config);
  server.on("/api/spoof", HTTP_POST, handle_spoof);
  server.on("/api/admin", HTTP_POST, handle_admin);
  server.on("/api/ota", HTTP_POST, handle_ota);
  server.on("/api/sta", HTTP_POST, handle_sta);  // v2.4 uplink test
  server.on("/api/cmd", HTTP_POST, handle_cmd);  // v2.4 console
  server.on("/api/backup", HTTP_GET, handle_backup);  // v2.4 config export
  server.on("/api/restore", HTTP_POST, handle_restore);  // v2.4 import
  server.on("/api/uprog", HTTP_GET, handle_uprog);  // v2.4 upload progress
  // Captive portal: any unknown host/path lands on the open dashboard.
  // This makes phones pop the dashboard on join and keeps "no internet"
  // devices from showing a dead 404. Reboot/reset/upload stay behind the
  // per-request admin password (no login wall since v2.3.1).
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302);
  });
  server.begin();
}

void web_tick(unsigned long now) {
  cfg_flush(now);  // coalesced NVS commit (see cfg_save)
  sta_tick(now);  // optional STA attempt resolves in the background
  sta_test_tick(now);  // v2.4 one-shot uplink test resolves here
  dns.processNextRequest();  // captive portal DNS (cheap; no-op off-AP)
  server.handleClient();     // short, non-blocking; RS485 keeps priority
}
