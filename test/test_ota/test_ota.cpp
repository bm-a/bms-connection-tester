#include <unity.h>
#include "../../src/ota.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// v2.3 OTA decision logic: version compare, asset pick, URL build, gate.

void test_ota_cmp_basic(void) {
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("2.3", "2.3"));
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("v2.3", "2.3"));
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("v2.3", "v2.3"));
  TEST_ASSERT_TRUE(ota_cmp_version("2.4", "2.3") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.3", "2.4") < 0);
}

void test_ota_cmp_multidigit(void) {
  TEST_ASSERT_TRUE(ota_cmp_version("2.10", "2.9") > 0);  // not lexicographic
  TEST_ASSERT_TRUE(ota_cmp_version("2.9", "2.10") < 0);
  TEST_ASSERT_TRUE(ota_cmp_version("10.0", "9.99") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.3.1", "2.3") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.3", "2.3.1") < 0);
}

void test_ota_cmp_garbage_safe(void) {
  // Garbage is never "newer" than a real release (fail closed: no update).
  TEST_ASSERT_TRUE(ota_cmp_version("", "2.3") < 0);
  TEST_ASSERT_TRUE(ota_cmp_version("latest", "2.3") <= 0);
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version(nullptr, nullptr));
}

void test_ota_asset_pick(void) {
  TEST_ASSERT_EQUAL_STRING("firmware.bin", ota_asset_for_variant("8mb"));
  TEST_ASSERT_EQUAL_STRING("n16r8-firmware.bin",
                           ota_asset_for_variant("n16r8"));
  TEST_ASSERT_EQUAL_STRING("firmware.bin", ota_asset_for_variant("bogus"));
  TEST_ASSERT_EQUAL_STRING("firmware.bin", ota_asset_for_variant(nullptr));
  // Compile-time default matches the plain 8 MB env.
  TEST_ASSERT_EQUAL_STRING("firmware.bin", ota_asset_for_variant(FW_VARIANT));
}

void test_ota_url_build(void) {
  char url[160];
  TEST_ASSERT_TRUE(ota_download_url("v2.4", "8mb", url, sizeof(url)));
  TEST_ASSERT_EQUAL_STRING(
      "https://github.com/bm-a/bms-connection-tester/releases/download/v2.4/"
      "firmware.bin",
      url);
  TEST_ASSERT_TRUE(ota_download_url("v2.4", "n16r8", url, sizeof(url)));
  TEST_ASSERT_TRUE(strstr(url, "n16r8-firmware.bin") != nullptr);
  // Rejects missing 'v', path tricks, tiny buffers.
  TEST_ASSERT_FALSE(ota_download_url("2.4", "8mb", url, sizeof(url)));
  TEST_ASSERT_FALSE(ota_download_url("v2.4/../evil", "8mb", url, sizeof(url)));
  TEST_ASSERT_FALSE(ota_download_url("v2.4", "8mb", url, 10));
  TEST_ASSERT_FALSE(ota_download_url(nullptr, "8mb", url, sizeof(url)));
}

void test_ota_gate(void) {
  const unsigned long DAY = 24UL * 3600UL * 1000UL;
  // Happy path: enabled, online, idle bench, silent bus, interval elapsed.
  TEST_ASSERT_TRUE(ota_should_check(DAY + 1000, 0, DAY, true, true, false,
                                    120000, 60000));
  // Each suppressor blocks.
  TEST_ASSERT_FALSE(ota_should_check(DAY + 1000, 0, DAY, false, true, false,
                                     120000, 60000));  // auto off
  TEST_ASSERT_FALSE(ota_should_check(DAY + 1000, 0, DAY, true, false, false,
                                     120000, 60000));  // STA offline
  TEST_ASSERT_FALSE(ota_should_check(DAY + 1000, 0, DAY, true, true, true,
                                     120000, 60000));  // sequence running
  TEST_ASSERT_FALSE(ota_should_check(DAY + 1000, 0, DAY, true, true, false,
                                     10000, 60000));  // bus was active
  TEST_ASSERT_FALSE(ota_should_check(1000, 0, DAY, true, true, false, 120000,
                                     60000));  // interval not elapsed
  TEST_ASSERT_FALSE(ota_should_check(DAY + 1000, 0, 0, true, true, false,
                                     120000, 60000));  // manual-only
  // Rollover-safe interval math.
  TEST_ASSERT_TRUE(ota_should_check(1000, 0xFFFFFFFFu - 1000u, 500, true,
                                    true, false, 120000, 60000));
}

void run_all() {
  RUN_TEST(test_ota_cmp_basic);
  RUN_TEST(test_ota_cmp_multidigit);
  RUN_TEST(test_ota_cmp_garbage_safe);
  RUN_TEST(test_ota_asset_pick);
  RUN_TEST(test_ota_url_build);
  RUN_TEST(test_ota_gate);
}

#ifdef ARDUINO
#include <Arduino.h>
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
