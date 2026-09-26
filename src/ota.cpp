#include "ota.h"
#include <string.h>
#include <stdio.h>

// Parse one numeric component; stops at '.' or end. Returns value, advances *p.
static unsigned long ota_num(const char **p) {
  unsigned long v = 0;
  while (**p >= '0' && **p <= '9') {
    v = v * 10u + (unsigned long)(**p - '0');
    (*p)++;
  }
  return v;
}

// Parse a trailing "-wsN" board-line suffix; true when present, *rev set.
static bool ota_ws_rev(const char **p, unsigned long *rev) {
  const char *q = *p;
  if (q[0] != '-' || q[1] != 'w' || q[2] != 's') return false;
  q += 3;
  if (*q < '0' || *q > '9') return false;
  unsigned long v = 0;
  while (*q >= '0' && *q <= '9') {
    v = v * 10u + (unsigned long)(*q - '0');
    q++;
  }
  *p = q;
  *rev = v;
  return true;
}

int ota_cmp_version(const char *a, const char *b) {
  if (!a) a = "";
  if (!b) b = "";
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;
  for (;;) {
    unsigned long va = ota_num(&a);
    unsigned long vb = ota_num(&b);
    if (va < vb) return -1;
    if (va > vb) return 1;
    bool ea = (*a != '.');
    bool eb = (*b != '.');
    if (ea && eb) {
      // Dotted parts equal: a "-wsN" board-line suffix decides within one
      // line (numeric, so ws10 > ws2). Suffixed vs plain is a different
      // product line — never newer, never older (no cross-line auto-update).
      unsigned long ra = 0, rb = 0;
      bool sa = ota_ws_rev(&a, &ra);
      bool sb = ota_ws_rev(&b, &rb);
      if (sa && sb) {
        if (ra < rb) return -1;
        if (ra > rb) return 1;
      }
      return 0;
    }
    if (ea) return -1;       // a ran out first -> shorter < longer
    if (eb) return 1;
    a++;
    b++;  // skip '.'
  }
}

bool ota_tag_is_waveshare_line(const char *tag) {
  // v<digits/dots>-ws<digits>, nothing else (strict: no path tricks).
  if (!tag) return false;
  if (*tag != 'v' && *tag != 'V') return false;
  tag++;
  bool any_digit = false;
  while ((*tag >= '0' && *tag <= '9') || *tag == '.') {
    if (*tag != '.') any_digit = true;
    tag++;
  }
  if (!any_digit) return false;
  if (tag[0] != '-' || tag[1] != 'w' || tag[2] != 's') return false;
  tag += 3;
  if (*tag < '0' || *tag > '9') return false;
  while (*tag >= '0' && *tag <= '9') tag++;
  return *tag == '\0';
}

const char *ota_asset_for_variant(const char *variant) {
  if (variant && strcmp(variant, "n16r8") == 0) return OTA_ASSET_N16R8;
  if (variant && strcmp(variant, "waveshare") == 0) return OTA_ASSET_WAVESHARE;
  return OTA_ASSET_8MB;
}

bool ota_download_url(const char *tag, const char *variant, char *out,
                      size_t out_len) {
  if (!tag || !out || out_len == 0) return false;
  if (tag[0] != 'v' && tag[0] != 'V') return false;
  // Tag sanity: v + digits/dots, with an optional -wsN board-line suffix
  // (e.g. v2.7-ws1). Nothing else — no path tricks from the network.
  const char *p = tag + 1;
  while ((*p >= '0' && *p <= '9') || *p == '.') p++;
  if (*p == '-' && p[1] == 'w' && p[2] == 's') {
    p += 3;
    if (*p < '0' || *p > '9') return false;
    while (*p >= '0' && *p <= '9') p++;
  }
  if (*p != '\0') return false;
  const char *asset = ota_asset_for_variant(variant);
  int n = snprintf(out, out_len,
                   "https://github.com/bm-a/bms-connection-tester/releases/"
                   "download/%s/%s",
                   tag, asset);
  return n > 0 && (size_t)n < out_len;
}

bool ota_should_check(unsigned long now, unsigned long last_check,
                      unsigned long interval_ms, bool auto_enabled,
                      bool sta_online, bool seq_running,
                      unsigned long bus_silent_ms,
                      unsigned long min_bus_silent_ms) {
  if (!auto_enabled) return false;
  if (!sta_online) return false;  // office has no internet: never spin
  if (seq_running) return false;  // don't steal the bench mid-cycle
  if (interval_ms == 0) return false;
  if (bus_silent_ms < min_bus_silent_ms) return false;
  return (now - last_check) >= interval_ms;  // unsigned: rollover-safe
}
