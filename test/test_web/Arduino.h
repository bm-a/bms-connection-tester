// Host stubs for Arduino core APIs used by web_ui.cpp (test_web ONLY).
// Faithful-enough semantics for logic testing: String ops, mock millis(),
// in-memory Preferences, fake WebServer with a request driver, WiFi no-ops.
// Never compiled into firmware (firmware uses the real Arduino core).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>

#define PROGMEM
#define HIGH 1
#define LOW 0

// ---- mock clock (inline = ONE shared instance across translation units) ----
inline unsigned long g_mock_millis = 0;
inline bool g_restart_requested = false;
inline unsigned long millis() { return g_mock_millis; }
inline void delay(unsigned long ms) { g_mock_millis += ms; }
inline uint32_t esp_random() {
  static uint32_t s = 0x12345678u;
  s = s * 1664525u + 1013904223u;
  return s;
}
struct EspStub {
  void restart() { g_restart_requested = true; }
};
inline EspStub ESP;

#define constrain(amt, low, high) \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// ---- String ----
class String {
 public:
  String() {}
  String(const char *c) : s_(c ? c : "") {}
  String(const std::string &x) : s_(x) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  size_t length() const { return s_.length(); }
  const char *c_str() const { return s_.c_str(); }
  int indexOf(const String &x) const { return indexOf(x.s_.c_str()); }
  int indexOf(const char *x) const {
    if (!x) return -1;
    auto p = s_.find(x);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(char c, unsigned from) const {
    auto p = s_.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(unsigned from) const {
    if (from >= s_.size()) return String();
    return String(s_.substr(from));
  }
  String substring(unsigned from, unsigned to) const {
    if (from >= s_.size() || to <= from) return String();
    return String(s_.substr(from, to - from));
  }
  long toInt() const {
    try {
      return std::stol(s_);
    } catch (...) {
      return 0;
    }
  }
  void toCharArray(char *buf, size_t n) const {
    if (n == 0) return;
    strncpy(buf, s_.c_str(), n - 1);
    buf[n - 1] = '\0';
  }
  String &operator+=(const String &o) {
    s_ += o.s_;
    return *this;
  }
  String &operator+=(const char *o) {
    if (o) s_ += o;
    return *this;
  }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  friend String operator+(const String &a, const String &b) {
    return String(a.s_ + b.s_);
  }
  friend String operator+(const char *a, const String &b) {
    return String(std::string(a ? a : "") + b.s_);
  }
  friend String operator+(const String &a, const char *b) {
    return String(a.s_ + std::string(b ? b : ""));
  }
  const std::string &stl() const { return s_; }

 private:
  std::string s_;
};
