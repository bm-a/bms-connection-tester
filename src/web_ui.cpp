#include "web_ui.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>

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
  nvs.end();
  if (ident.ap_channel < 1 || ident.ap_channel > 13) ident.ap_channel = 6;
  // Clamp everything a hand-edited NVS (or older UI) could have stored.
  if (c.relay_count < 1 || c.relay_count > RELAY_COUNT) c.relay_count = 8;
  if (c.relay_mode > RELAY_CHASE) c.relay_mode = RELAY_SEQUENTIAL;
  if (c.button_mode > BTN_RESTART) c.button_mode = BTN_HOLD_ABORT;
  if (c.seq_dir > 1) c.seq_dir = 0;
  if (c.cycle_pause_ms > 3600000UL) c.cycle_pause_ms = 3600000UL;
  if (c.allon_stagger_ms > 60000) c.allon_stagger_ms = 60000;
}

void web_factory_reset() {
  cfg_dirty = false;
  nvs.begin(WEB_NVS_NS, false);
  nvs.clear();
  nvs.end();
  delay(200);
  ESP.restart();
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
<div class=row><label>Mode <select id=rmode><option value=0>Sequential 1-N</option><option value=2>Chase wave</option><option value=1>All ON at once</option></select></label>
<label>Relays <input id=nrel size=3></label>
<label>Step ms <input id=step size=6></label></div>
<div class=row><label>Hold seq ms (0=&infin;) <input id=hseq size=7></label>
<label>Chase sweeps (0=&infin;) <input id=swp size=4></label>
<label>Hold all-on ms <input id=hall size=7></label></div>
<div class=row><label>Button <select id=bmode><option value=0>Hold X ms, re-press=OFF</option><option value=1>Run to end, ignore presses</option><option value=2>Re-press restarts</option></select></label>
<label>Logic <select id=alow><option value=1>Active-LOW (SmartElex)</option><option value=0>Active-HIGH</option></select></label>
<label>Direction <select id=dir><option value=0>R1&rarr;Rn</option><option value=1>Rn&rarr;R1</option></select></label></div>
<div class=row><label><input type=checkbox id=loop> Loop cycles</label>
<label>Pause ms <input id=cpause size=7></label>
<label>Cycle limit (0=&infin;) <input id=clim size=5></label>
<label>All-ON stagger ms <input id=stag size=5></label></div>
<div class=row><button onclick="saveCfg()">Save</button><span class=msg id=cfgmsg></span></div></div>
<div class=card><h3>Relay labels</h3>
<div class=row id=labels></div>
<div class=row><button onclick="saveLabels()">Save labels</button><span class=msg id=lblmsg></span></div></div>
<div class=card><h3>Fault spoof (0x03 test values, stage 1 then 2)</h3>
<div class=row><label><input type=checkbox id=sena> pin-trigger enabled</label><label>Trigger GPIO <input id=spin size=3></label><span id=spoofmsg class=spoof-on></span></div>
<div class=row><span class=note>GPIO + values save on FIRE. Safe pins: 1, 2, 21, 38-44, 47 (else 21).</span></div>
<div class=row><label>1: V <input id=sv size=5></label><label>A <input id=sa size=5></label><label>&deg;C <input id=sc size=5></label><label>SOC% <input id=ssoc size=4></label><label>Secs <input id=ssec size=4></label></div>
<div class=row><label>2: V <input id=s2v size=5></label><label>A <input id=s2a size=5></label><label>&deg;C <input id=s2c size=5></label><label>SOC% <input id=s2soc size=4></label><label>Secs <input id=s2sec size=4></label></div>
<div class=row><button class=warn onclick="spoof('fire')">FIRE now</button><button onclick="spoof('cancel')">Cancel</button></div></div>
<div class=card><h3>Firmware update</h3>
<div class=row><span id=ota_status class=note></span><span id=ota_latest class=note></span></div>
<div class=row><label><input type=checkbox id=ota_auto> Auto-check GitHub daily (needs STA uplink below)</label>
<button onclick="ota('check')">Check now</button><button id=otainstall style="display:none" class=warn onclick="ota('install')">Install update</button><a href=/update>Firmware upload</a><span class=msg id=otamsg></span></div>
<div class=row><label><input type=checkbox id=sta_en> STA uplink (hotspot for internet)</label>
<label>SSID <input id=sta_ssid size=12></label><label>Pass <input id=sta_pass type=password size=12 placeholder="blank = keep"></label>
<button onclick="saveAdmin()">Save</button><span class=note id=stamsg></span></div></div>
<div class=card><h3>Admin &amp; Wi-Fi AP</h3>
<div class=row><label>SSID <input id=ap_ssid size=14></label><label>Pass (8+, blank = keep) <input id=ap_pass type=password size=14 placeholder="blank = keep"></label><label>Ch <input id=ap_ch size=3></label></div>
<div class=row><label>New admin pass (4+, blank = keep) <input id=a_pass type=password size=14 placeholder="blank = keep"></label></div>
<div class=row><span class=note>Reboot, factory reset, update upload and these saves ask for the admin password (default admin123).</span></div>
<div class=row><label><input type=checkbox id=auto> Auto-start sequence on boot (burn-in)</label></div>
<div class=row><button onclick="saveAdmin()">Save</button>
<button class=warn onclick="if(confirm('Reboot?'))admin('reboot')">Reboot</button>
<button class=danger onclick="if(confirm('Factory reset?'))admin('reset')">Factory reset</button><span class=msg id=admmsg></span></div></div>
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
 return s;
}
function fillForm(s){if(!s||!s.cfg)return;
 let lb=document.getElementById('labels');if(lb&&lb.children.length===0){let h='';for(let i=0;i<8;i++)h+=`<label>R${i+1} <input id="lbl${i}" size=8></label>`;lb.innerHTML=h;}
 for(let k of ['rmode','nrel','step','hseq','swp','hall','bmode','alow','dir','loop','cpause','clim','stag','auto','sena','sv','sa','sc','ssoc','ssec','s2v','s2a','s2c','s2soc','s2sec','spin','ota_auto','sta_en','sta_ssid','ap_ssid','ap_ch','lbl0','lbl1','lbl2','lbl3','lbl4','lbl5','lbl6','lbl7']){let e=document.getElementById(k);if(e&&!dirty[k]&&document.activeElement!==e){if(e.type==='checkbox')e.checked=!!s.cfg[k];else e.value=(s.cfg[k]===undefined?'':s.cfg[k]);}}
}
async function loadForm(){let s;try{s=await jget('/api/state');}catch(e){return;}fillForm(s);}
async function relay(i,on){if(i>=NREL)return;await jpost('/api/relay',{i,on});refresh();}
async function seq(c){await jpost('/api/seq',{cmd:c});refresh();}
async function saveCfg(){let ks=['rmode','nrel','step','hseq','swp','hall','bmode','alow','dir','cpause','clim','stag'];let b={};for(let k of ks){b[k]=+document.getElementById(k).value;}b.loop=document.getElementById('loop').checked?1:0;let r=await jpost('/api/config',b);document.getElementById('cfgmsg').textContent=r.ok?'saved':'ERR';if(r.ok){clean(ks);clean(['loop']);loadForm();}refresh();}
async function saveLabels(){let ks=[];for(let i=0;i<8;i++)ks.push('lbl'+i);let b={};for(let k of ks)b[k]=document.getElementById(k).value;let r=await jpost('/api/config',b);document.getElementById('lblmsg').textContent=r.ok?'saved':'ERR';if(r.ok){clean(ks);loadForm();}refresh();}
async function spoof(c){let ks=['sv','sa','sc','ssoc','ssec','s2v','s2a','s2c','s2soc','s2sec','spin'];let b={cmd:c};if(c==='fire'){for(let k of ks)b[k]=+document.getElementById(k).value;b.sena=document.getElementById('sena').checked?1:0;}let r=await jpost('/api/spoof',b);document.getElementById('spoofmsg').textContent=r.ok?'ok':'ERR';if(r.ok&&c==='fire'){clean(ks);clean(['sena']);loadForm();}refresh();}
async function ota(c){let p=prompt('Admin password:','');if(p===null)return;document.getElementById('otamsg').textContent=(c==='install'?'installing — box reboots on success':'checking…');let r=await jpost('/api/ota',{cmd:c,pass:p});document.getElementById('otamsg').textContent=r.ok?(c==='install'?'install started':'started'):'ERR: '+(r.err||'');refresh();}
async function saveAdmin(){let ks=['ap_ssid','ap_pass','ap_ch','a_pass','sta_ssid','sta_pass'];let b={};for(let k of ks)b[k]=document.getElementById(k).value;b.sta_en=document.getElementById('sta_en').checked?1:0;b.auto=document.getElementById('auto').checked?1:0;let p=prompt('Admin password:','');if(p===null)return;b.pass=p;let bb={pass:p};for(let k of ['ota_auto'])bb[k]=document.getElementById(k).checked?1:0;await jpost('/api/ota',bb);let r=await jpost('/api/admin',b);document.getElementById('admmsg').textContent=r.ok?'saved (reboot to apply AP/STA)':'ERR: '+r.err;if(r.ok){clean(ks);clean(['sta_en','auto','ota_auto']);document.getElementById('ap_pass').value='';document.getElementById('a_pass').value='';document.getElementById('sta_pass').value='';loadForm();}refresh();}
async function admin(c){let p=prompt('Admin password:','');if(p===null)return;let r=await jpost('/api/admin',{cmd:c,pass:p});if(!r.ok)alert('ERR: '+(r.err||''));}
setInterval(refresh,1000);refresh();loadForm();
</script></body></html>)HTML";

// ---- route helpers ----
static void send_json(const String &s) {
  server.send(200, "application/json", s);
}

static void handle_root() {
  server.send_P(200, "text/html", PAGE_DASH);
}

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
  if (jstr(b, "cmd") == "start") G->seq->start(millis());
  else G->seq->stopAll();
  send_json("{\"ok\":1}");
}

static void handle_config() {
  Bms2Config &c = *G->cfg;
  String b = server.arg("plain");
  if (has(b, "rmode")) {
    long m = jnum(b, "rmode", 0);
    c.relay_mode = (m < 0 || m > 2) ? RELAY_SEQUENTIAL : (uint8_t)m;
  }
  if (has(b, "nrel"))
    c.relay_count = (uint8_t)constrain(jnum(b, "nrel", 8), 1, RELAY_COUNT);
  if (has(b, "step")) c.step_delay_ms = (uint16_t)constrain(jnum(b, "step", 500), 50, 60000);
  if (has(b, "hseq")) c.hold_seq_ms = (uint32_t)constrain(jnum(b, "hseq", 30000), 0, 3600000);
  if (has(b, "swp")) c.chase_sweeps = (uint8_t)constrain(jnum(b, "swp", 3), 0, 100);
  if (has(b, "hall")) c.hold_all_ms = (uint32_t)constrain(jnum(b, "hall", 300000), 0, 3600000);
  if (has(b, "bmode")) {
    long m = jnum(b, "bmode", 0);
    c.button_mode = (m < 0 || m > 2) ? BTN_HOLD_ABORT : (uint8_t)m;
  }
  if (has(b, "alow")) c.active_low = jnum(b, "alow", 1) != 0;
  if (has(b, "loop")) c.loop_enabled = jnum(b, "loop", 0) != 0;
  if (has(b, "cpause"))
    c.cycle_pause_ms = (uint32_t)constrain(jnum(b, "cpause", 5000), 0, 3600000);
  if (has(b, "clim"))
    c.cycle_limit = (uint16_t)constrain(jnum(b, "clim", 0), 0, 60000);
  if (has(b, "stag"))
    c.allon_stagger_ms = (uint16_t)constrain(jnum(b, "stag", 0), 0, 60000);
  if (has(b, "dir")) c.seq_dir = (jnum(b, "dir", 0) == 1) ? 1 : 0;
  if (has(b, "auto")) c.boot_autostart = jnum(b, "auto", 0) != 0;
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
  send_json("{\"ok\":1}");
}

static void handle_spoof() {
  Bms2Config &c = *G->cfg;
  String b = server.arg("plain");
  String cmd = jstr(b, "cmd");
  if (cmd == "cancel") { G->spoof->cancel(); send_json("{\"ok\":1}"); return; }
  // fire: optional inline values for BOTH stages (x0.1 display units),
  // then trigger the two-stage plan.
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
  cfg_save();
  if (G->on_config_changed) G->on_config_changed();
  if (cmd == "fire")
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
  if (cmd == "reboot") {
    cfg_commit();  // flush any pending save first: reboot wipes RAM
    send_json("{\"ok\":1}");
    delay(200);
    ESP.restart();
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
  send_json("{\"ok\":1}");
}

// ---- v2.3 manual firmware upload (works fully offline) ----
static const char PAGE_UPDATE[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>BMS Tester firmware update</title><style>body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#e2e8f0}form{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:24px}input[type=submit]{background:#b45309;border:0;color:#fff;border-radius:8px;padding:10px 16px;font-size:15px;cursor:pointer}a{color:#60a5fa}</style></head><body style="max-width:480px;margin:60px auto;padding:12px">
<h2>Firmware update</h2>
<p>Upload a <b>.bin</b> from the GitHub release matching this box
(<b>firmware.bin</b> for 8 MB boards, <b>n16r8-firmware.bin</b> for N16R8).
The box reboots on success. Wrong file = re-flash over USB.</p>
<form method=POST action=/update enctype=multipart/form-data>
Admin password<br><input name=pass type=password><br><br>
<input type=file name=firmware accept=.bin><br><br>
<input type=submit value="Upload + reboot">
</form><p><a href=/>Back to dashboard</a></p></body></html>)HTML";

static void handle_update_page() {
  server.send_P(200, "text/html", PAGE_UPDATE);
}

static String upload_pass = "";
static bool upload_rejected = false;
static bool upload_begun = false;

static void handle_update_upload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (up.filename.length() == 0) return;  // form-field part, not the file
    // The pass field precedes the file in the form, so it is buffered by now.
    upload_rejected = !admin_ok(upload_pass);
    if (!upload_rejected) {
      if (Update.begin(UPDATE_SIZE_UNKNOWN)) upload_begun = true;
      else upload_rejected = true;
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (up.filename.length() == 0) {
      if (up.name == "pass" && upload_pass.length() < 64) {
        // Append as a 1-char C string: portable across Arduino String and
        // the host stub (a raw char would bind String(int) on the stub).
        char ch[2] = {'\0', '\0'};
        for (size_t i = 0; i < up.currentSize; i++) {
          ch[0] = (char)up.buf[i];
          upload_pass += ch;
        }
      }
      return;
    }
    if (upload_rejected || !upload_begun) return;  // swallow: commit nothing
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.end(false);
      upload_begun = false;
      upload_rejected = true;
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (upload_begun && !upload_rejected) Update.end(true);
    upload_begun = false;
  }
}

static void handle_update_done() {
  // Both the streamed field and the parsed arg must agree (defense in depth).
  bool ok = !upload_rejected && admin_ok(upload_pass) &&
            admin_ok(server.arg("pass"));
  upload_pass = "";
  upload_rejected = false;
  if (upload_begun) { Update.end(ok); upload_begun = false; }
  if (!ok) {
    server.send(403, "text/plain", "admin password required");
    return;
  }
  if (Update.hasError()) {
    server.send(200, "text/plain", "UPDATE FAILED");
    return;
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "UPDATE OK - rebooting");
  delay(200);
  ESP.restart();
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
  sta_begin();  // optional uplink attempt (non-blocking; AP unaffected)
  server.begin();  // re-begin is safe: routes were registered once in setup
  wifi_is_up = true;
}

// v2.3.1 WiFi kill switch: grounding the pin drops everything now;
// releasing it brings the AP back (same SSID/pass/channel).
void web_wifi_set(bool on) {
  if (on == wifi_is_up) return;
  if (!on) {
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
  sta_state = 0;
  cfg_dirty = false;  // fresh boot: nothing pending
  cfg_load();  // NVS -> cfg + identity (or defaults)
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
  dns.processNextRequest();  // captive portal DNS (cheap; no-op off-AP)
  server.handleClient();     // short, non-blocking; RS485 keeps priority
}

#endif  // ARDUINO
