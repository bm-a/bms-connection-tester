#include <unity.h>
#include <cstring>
#include "../../src/bms_protocol.h"

void setUp(void) {}
void tearDown(void) {}

// Exhaustive: every single-byte value at every position of the 7-byte
// request must not emit, except the exact bytes themselves.
void test_exhaustive_single_byte_corruptions(void) {
  uint8_t good[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  int checked = 0;
  for (int pos = 0; pos < 7; pos++) {
    for (int v = 0; v < 256; v++) {
      if (v == good[pos]) continue;
      if (pos == 1 && v == 0x5A) {
        // DD 5A 03 00 FF FD 77 is a *valid write* to reg 0x03
        // (same checksum): parser must emit it flagged as write...
        uint8_t w[7];
        memcpy(w, good, 7);
        w[1] = 0x5A;
        JbdFrame f;
        JbdParser p;
        bool hit = false;
        for (int j = 0; j < 7; j++) hit |= p.feed(w[j], f);
        TEST_ASSERT_TRUE(hit);
        TEST_ASSERT_TRUE(f.is_write);
        // ...and the dispatcher must stay silent (option A).
        size_t rl = 0;
        TEST_ASSERT_NULL(reply_for(f.reg, f.is_write, rl));
        checked++;
        continue;
      }
      uint8_t bad[7];
      memcpy(bad, good, 7);
      bad[pos] = (uint8_t)v;
      JbdFrame f;
      JbdParser p;
      bool hit = false;
      for (int j = 0; j < 7; j++) hit |= p.feed(bad[j], f);
      if (hit) {
        printf("FALSE EMIT pos=%d val=%02X\n", pos, v);
        TEST_FAIL();
        return;
      }
      checked++;
    }
  }
  printf("exhaustive corruptions checked: %d\n", checked);
  TEST_ASSERT_EQUAL_INT(7 * 255, checked);
}

// 10M structured fuzz: pure random + valid-prefix mutations.
void test_fuzz_10m(void) {
  JbdParser p;
  JbdFrame f;
  uint32_t s = 0xC0FFEE;
  long hits = 0;
  for (long i = 0; i < 10000000L; i++) {
    s = s * 1664525u + 1013904223u;
    uint8_t b;
    int mode = (s >> 24) & 7;
    if (mode < 2) {
      // mutate valid frames: random 1-2 byte changes
      static const uint8_t g[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
      b = g[(s >> 8) % 7] ^ (uint8_t)(1 + ((s >> 16) & 0xFF));
      if (b == g[(s >> 8) % 7]) b ^= 0x80;
    } else {
      b = (s >> 16) & 0xFF;
    }
    if (p.feed(b, f)) hits++;
  }
  printf("10M fuzz emits: %ld\n", hits);
  TEST_ASSERT_EQUAL_INT(0, hits);
}

// Cadence x register sweep: 100ms..10s periods, regs 03/04/05, plus
// writes and unknown-reg interleaved (must refresh green, stay silent).
void test_cadence_register_sweep(void) {
  const unsigned long periods[] = {100, 250, 500, 1000, 2000, 2500, 5000, 8000, 10000};
  const uint8_t regs[] = {0x03, 0x04, 0x05};
  for (unsigned pi = 0; pi < sizeof(periods) / sizeof(periods[0]); pi++) {
    for (unsigned ri = 0; ri < 3; ri++) {
      JbdParser p;
      PollTracker t;
      JbdFrame f;
      uint8_t reg = regs[ri];
      uint16_t ck = (uint16_t)(0x10000u - reg);
      uint8_t req[7] = {0xDD, 0xA5, reg, 0x00, (uint8_t)(ck >> 8), (uint8_t)(ck & 0xFF), 0x77};
      unsigned long now = 5000;
      // 12 polls at this cadence
      for (int k = 0; k < 12; k++) {
        int hits = 0;
        for (int i = 0; i < 7; i++)
          if (p.feed(req[i], f)) { hits++; }
        TEST_ASSERT_EQUAL_INT(1, hits);
        t.note_poll(now);
        size_t rl = 0;
        TEST_ASSERT_NOT_NULL(reply_for(f.reg, f.is_write, rl));
        // interleave a write + unknown read: green refresh, no reply
        uint8_t w[9] = {0xDD, 0x5A, 0x10, 0x02, 0xAA, 0x55, 0xFE, 0xEF, 0x77};
        for (int i = 0; i < 9; i++)
          if (p.feed(w[i], f)) {
            t.note_poll(now);
            TEST_ASSERT_NULL(reply_for(f.reg, f.is_write, rl));
          }
        now += periods[pi];
      }
      // mid-run must be green for every cadence (adaptive window covers all)
      TEST_ASSERT_TRUE(t.active(now - periods[pi] + 100));
    }
  }
}

// Rapid-fire: polls back-to-back with no gap (bus saturation).
void test_bus_saturation(void) {
  JbdParser p;
  PollTracker t;
  JbdFrame f;
  uint8_t req[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  unsigned long now = 1000;
  int frames = 0, replies = 0;
  for (int k = 0; k < 5000; k++) {
    for (int i = 0; i < 7; i++)
      if (p.feed(req[i], f)) {
        frames++;
        t.note_poll(now);
        size_t rl = 0;
        if (reply_for(f.reg, f.is_write, rl)) replies++;
      }
    now += 5;  // 5 ms apart: faster than a 35 ms reply (overlap stress)
  }
  TEST_ASSERT_EQUAL_INT(5000, frames);
  TEST_ASSERT_EQUAL_INT(5000, replies);
  TEST_ASSERT_TRUE(t.active(now));
}

void run_all() {
  RUN_TEST(test_exhaustive_single_byte_corruptions);
  RUN_TEST(test_fuzz_10m);
  RUN_TEST(test_cadence_register_sweep);
  RUN_TEST(test_bus_saturation);
}

#ifdef ARDUINO
void setup() {
  delay(1000);
  UNITY_BEGIN();
  run_all();
  UNITY_END();
}
void loop() {}
#else
int main() {
  UNITY_BEGIN();
  run_all();
  return UNITY_END();
}
#endif
