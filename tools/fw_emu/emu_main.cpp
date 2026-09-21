// Firmware emulation harness: the REAL web_ui.cpp + relay + spoof + OTA
// decision logic, serving real HTTP on 127.0.0.1:FW_EMU_PORT (default 8080)
// via the POSIX-socket WebServer shim. Deterministic virtual time + link
// state are driven through the side control port (FW_EMU_CTL, default 8081):
//   GET /__time?ms=N   set clock, run web_tick + seq.tick at N
//   GET /__link?up=1   STA link up (0 = down)
//   GET /__update       Update-stub state (finished/error/end_calls/bytes)
//   GET /__flags        ota install/check calls + restart flag (then clear)
//   GET /__chip?sketch=N&flash=N  free-sketch / flash size overrides
// Emu-only. Virtual time NEVER advances on its own: every millisecond of
// firmware behavior is explicitly stepped by the driver scripts.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "Arduino.h"
#include "Preferences.h"
#include "Update.h"
#include "WiFi.h"
#include "../../src/relay_ctrl.h"
#include "../../src/ota.h"
#include "../../src/web_ui.h"

static Bms2Config g_cfg;
static RelaySequencer g_seq;
static SpoofPlan g_spoof;
static OtaState g_ota;
static bool g_link = false;
static uint8_t g_frame_a[SPOOF_FRAME_LEN];
static uint8_t g_frame_b[SPOOF_FRAME_LEN];
static bool g_ota_check = false;
static bool g_ota_install = false;

static void rebuild_frames() {
  build_spoof_frame(g_cfg, 1, g_frame_a);
  build_spoof_frame(g_cfg, 2, g_frame_b);
}

static int ctl_fd = -1;

static void ctl_begin(int port) {
  ctl_fd = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(ctl_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons((uint16_t)port);
  bind(ctl_fd, (sockaddr *)&a, sizeof(a));
  listen(ctl_fd, 4);
}

static std::string ctl_read(int fd) {
  std::string out;
  char buf[4096];
  while (out.find("\r\n\r\n") == std::string::npos && out.size() < 65536) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeval tv{2, 0};
    if (select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) break;
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    out.append(buf, (size_t)n);
  }
  return out;
}

static void ctl_poll() {
  if (ctl_fd < 0) return;
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(ctl_fd, &fds);
  timeval tv{0, 5000};
  if (select(ctl_fd + 1, &fds, nullptr, nullptr, &tv) <= 0) return;
  int c = accept(ctl_fd, nullptr, nullptr);
  if (c < 0) return;
  std::string req = ctl_read(c);
  std::string target = "/";
  {
    size_t s1 = req.find(' ');
    size_t s2 = req.find(' ', s1 + 1);
    if (s1 != std::string::npos && s2 != std::string::npos)
      target = req.substr(s1 + 1, s2 - s1 - 1);
  }
  std::string path = target, query;
  size_t qm = target.find('?');
  if (qm != std::string::npos) {
    path = target.substr(0, qm);
    query = target.substr(qm + 1);
  }
  auto q = [&](const char *k) -> std::string {
    std::string key = std::string(k) + "=";
    size_t p = query.find(key);
    if (p == std::string::npos) return "";
    size_t e = query.find('&', p);
    return query.substr(p + key.size(),
                        e == std::string::npos ? e : e - p - key.size());
  };
  char body[512] = "";
  if (path == "/__clock") {
    snprintf(body, sizeof(body), "{\"ms\":%lu}", g_mock_millis);
  } else   if (path == "/__time") {
    unsigned long target = (unsigned long)atol(q("ms").c_str());
    unsigned long grain = (unsigned long)atol(q("tick_ms").c_str());
    // Fine-grain mode: walk the clock in grain steps so phase machines
    // (hold expiry, pause restart) advance exactly like hardware ticks.
    // Default (grain 0): one tick at the target (exact single-stepping).
    if (grain > 0 && target > g_mock_millis) {
      for (unsigned long t = g_mock_millis + grain; t < target; t += grain) {
        g_mock_millis = t;
        web_tick(t);
        g_seq.tick(t);
      }
    }
    g_mock_millis = target;
    web_tick(target);
    g_seq.tick(target);
    snprintf(body, sizeof(body), "{\"ok\":1,\"ms\":%lu}", g_mock_millis);
  } else if (path == "/__link") {
    g_wifi_status = (q("up") == "1") ? WL_CONNECTED : WL_DISCONNECTED;
    web_tick(g_mock_millis);
    snprintf(body, sizeof(body), "{\"ok\":1,\"link\":%d}", g_wifi_status);
  } else if (path == "/__update") {
    snprintf(body, sizeof(body),
             "{\"finished\":%d,\"error\":%d,\"ends\":%d,\"bytes\":%u,"
             "\"begin_size\":%u}",
             Update.finished ? 1 : 0, Update.error ? 1 : 0, Update.end_calls,
             (unsigned)Update.bytes.size(), (unsigned)Update.begin_size);
  } else if (path == "/__flags") {
    snprintf(body, sizeof(body),
             "{\"ota_check\":%d,\"ota_install\":%d,\"restart\":%d}",
             g_ota_check ? 1 : 0, g_ota_install ? 1 : 0,
             g_restart_requested ? 1 : 0);
    g_ota_check = g_ota_install = false;
    g_restart_requested = false;
  } else if (path == "/__nvs") {
    snprintf(body, sizeof(body), "{\"commits\":%u}",
             Preferences::nvs_commits());
  } else if (path == "/__chip") {    if (!q("sketch").empty())
      g_esp_free_sketch = (uint32_t)atol(q("sketch").c_str());
    if (!q("flash").empty())
      g_esp_flash_size = (uint32_t)atol(q("flash").c_str());
    snprintf(body, sizeof(body), "{\"ok\":1}");
  } else {
    snprintf(body, sizeof(body), "{\"ok\":0,\"err\":\"unknown\"}");
  }
  std::string out = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                    "Content-Length: " +
                    std::to_string(strlen(body)) +
                    "\r\nConnection: close\r\n\r\n" + body;
  size_t off = 0;
  while (off < out.size()) {
    ssize_t n = send(c, out.data() + off, out.size() - off, 0);
    if (n <= 0) break;
    off += (size_t)n;
  }
  close(c);
}

int main() {
  WebCtx ctx;
  ctx.cfg = &g_cfg;
  ctx.seq = &g_seq;
  ctx.spoof = &g_spoof;
  ctx.link_green = &g_link;
  ctx.on_config_changed = rebuild_frames;
  ctx.ota = &g_ota;
  ctx.on_ota_check = []() { g_ota_check = true; };
  ctx.on_ota_install = []() { g_ota_install = true; };
  g_mock_millis = 1000000UL;
  g_seq.begin(&g_cfg);  // like main.cpp setup(): sequencer needs its cfg
  web_setup(ctx);
  const char *cp = getenv("FW_EMU_CTL");
  ctl_begin(cp ? atoi(cp) : 8081);
  printf("fw_emu up: fw=%s ctl=%s\n",
         getenv("FW_EMU_PORT") ? getenv("FW_EMU_PORT") : "8080",
         cp ? cp : "8081");
  fflush(stdout);
  for (;;) {
    web_tick(g_mock_millis);  // serves the firmware port (non-blocking)
    ctl_poll();               // serves the control port (non-blocking)
    usleep(5000);
  }
  return 0;
}
