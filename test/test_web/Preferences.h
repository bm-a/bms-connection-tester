// Host stub: Preferences NVS (test_web ONLY). In-memory namespaced store
// that survives begin()/end() cycles, like real flash-backed NVS.
#pragma once
#include "Arduino.h"

class Preferences {
 public:
  bool begin(const char *ns, bool) {
    ns_ = ns ? ns : "";
    return true;
  }
  void end() {}
  void clear() {
    store()[ns_].clear();
    nums()[ns_].clear();
  }
  uint8_t getUChar(const char *k, uint8_t d) {
    return (uint8_t)getNum(k, d);
  }
  uint16_t getUShort(const char *k, uint16_t d) {
    return (uint16_t)getNum(k, d);
  }
  bool getBool(const char *k, bool d) { return getNum(k, d ? 1 : 0) != 0; }
  String getString(const char *k, const char *d) {
    auto &m = store()[ns_];
    auto it = m.find(k ? k : "");
    if (it == m.end()) return String(d);
    return String(it->second);
  }
  void putUChar(const char *k, uint8_t v) { putNum(k, v); }
  void putUShort(const char *k, uint16_t v) { putNum(k, v); }
  void putBool(const char *k, bool v) { putNum(k, v ? 1 : 0); }
  void putString(const char *k, const char *v) {
    store()[ns_][k ? k : ""] = v ? v : "";
  }

 private:
  std::string ns_;
  static std::map<std::string, std::map<std::string, unsigned long>> &nums() {
    static std::map<std::string, std::map<std::string, unsigned long>> m;
    return m;
  }
  static std::map<std::string, std::map<std::string, std::string>> &store() {
    static std::map<std::string, std::map<std::string, std::string>> m;
    return m;
  }
  unsigned long getNum(const char *k, unsigned long d) {
    auto &m = nums()[ns_];
    auto it = m.find(k ? k : "");
    return it == m.end() ? d : it->second;
  }
  void putNum(const char *k, unsigned long v) { nums()[ns_][k ? k : ""] = v; }
};
