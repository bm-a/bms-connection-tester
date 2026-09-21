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
    if (ea && eb) return 0;  // same length, all equal
    if (ea) return -1;       // a ran out first -> shorter < longer
    if (eb) return 1;
    a++;
    b++;  // skip '.'
  }
}

const char *ota_asset_for_variant(const char *variant) {
  if (variant && strcmp(variant, "n16r8") == 0) return OTA_ASSET_N16R8;
  return OTA_ASSET_8MB;
}

bool ota_download_url(const char *tag, const char *variant, char *out,
                      size_t out_len) {
  if (!tag || !out || out_len == 0) return false;
  if (tag[0] != 'v' && tag[0] != 'V') return false;
  // Tag sanity: v + digits/dots only (no path tricks from the network).
  for (const char *p = tag + 1; *p; p++) {
    if (!((*p >= '0' && *p <= '9') || *p == '.')) return false;
  }
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
