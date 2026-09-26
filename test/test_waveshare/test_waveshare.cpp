#include <unity.h>
#include "../../src/waveshare_pins.h"
#include "../../src/relay_ctrl.h"
#include "../../src/ota.h"
#include "../../src/fw_upload.h"

void setUp(void) {}
void tearDown(void) {}

// Waveshare ESP32-S3-ETH-8DI-8RO board backend. Compiled with
// -DBOARD_WAVESHARE_8DI8RO (see run_tests.sh). The generic-path tests in
// test_relay/test_ota/test_upload compile WITHOUT the flag and must be
// unaffected by these board-specific branches.

// ---- pin map (verified against wiki + vendor demo code) ----

void test_ws_pin_map(void) {
  TEST_ASSERT_EQUAL_UINT8(0x20, WS_TCA9554_ADDR);
  TEST_ASSERT_EQUAL_UINT8(42, WS_I2C_SDA);
  TEST_ASSERT_EQUAL_UINT8(41, WS_I2C_SCL);
  TEST_ASSERT_EQUAL_UINT8(0x01, WS_TCA9554_REG_OUTPUT);
  TEST_ASSERT_EQUAL_UINT8(0x03, WS_TCA9554_REG_CONFIG);
  TEST_ASSERT_EQUAL_UINT8(17, WS_PIN_RS485_TX);
  TEST_ASSERT_EQUAL_UINT8(18, WS_PIN_RS485_RX);
  TEST_ASSERT_EQUAL_UINT8(0, WS_PIN_BUTTON);
  TEST_ASSERT_EQUAL_UINT8(4, WS_PIN_DI_BASE);
  TEST_ASSERT_EQUAL_UINT8(8, WS_PIN_DI_COUNT);
  TEST_ASSERT_EQUAL_UINT8(4, WS_PIN_SPOOF);
  TEST_ASSERT_EQUAL_UINT8(5, WS_PIN_WIFI_KILL);
  TEST_ASSERT_EQUAL_UINT8(38, WS_PIN_RGB);
}

// ---- TCA9554 output byte: EXIO(i+1) = bit i, HIGH = ON ----

void test_ws_output_byte_all_on_off(void) {
  const bool all_on[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  const bool all_off[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8(0xFF, ws_output_byte(all_on, 8));
  TEST_ASSERT_EQUAL_UINT8(0x00, ws_output_byte(all_off, 8));
}

void test_ws_output_byte_single_bits(void) {
  // Relay R1 = bit 0 ... R8 = bit 7 (matches vendor Set_EXIO shift).
  for (uint8_t i = 0; i < 8; i++) {
    bool on[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    on[i] = true;
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(1u << i), ws_output_byte(on, 8));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(1u << i), ws_relay_bit(i));
  }
}

void test_ws_output_byte_count_clipping(void) {
  // Relays beyond relay_count are forced OFF even if logically on —
  // same rule as the direct-GPIO path.
  const bool all_on[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  TEST_ASSERT_EQUAL_UINT8(0x0F, ws_output_byte(all_on, 4));
  TEST_ASSERT_EQUAL_UINT8(0x01, ws_output_byte(all_on, 1));
  TEST_ASSERT_EQUAL_UINT8(0x00, ws_output_byte(all_on, 0));
  // Clamp: count > 8 behaves as 8.
  TEST_ASSERT_EQUAL_UINT8(0xFF, ws_output_byte(all_on, 9));
  TEST_ASSERT_EQUAL_UINT8(0xFF, ws_output_byte(all_on, 255));
}

void test_ws_output_byte_ignores_active_low(void) {
  // The expander stage is fixed HIGH-bit = ON: there is no polarity input
  // to ws_output_byte at all (verified at compile time — the signature
  // takes only logical states + count), so dashboard labels always match.
  const bool mixed[8] = {1, 0, 1, 0, 0, 0, 0, 1};
  TEST_ASSERT_EQUAL_UINT8(0x85, ws_output_byte(mixed, 8));
}

// ---- spoof trigger allowlist: DI terminals only ----

void test_ws_spoof_pin_allowlist(void) {
  const uint8_t ok[] = {4, 5, 6, 7, 8, 9, 10, 11};
  for (uint8_t i = 0; i < sizeof(ok); i++)
    TEST_ASSERT_EQUAL_UINT8(ok[i], sanitize_spoof_pin(ok[i]));
  // Button, Ethernet, RS485, I2C, RGB, buzzer, strapping, old defaults:
  // all fall back to DI1.
  const uint8_t bad[] = {0, 1, 2, 3, 12, 13, 14, 15, 16, 17, 18, 21,
                         38, 39, 40, 41, 42, 43, 44, 46, 47, 48, 99};
  for (uint8_t i = 0; i < sizeof(bad); i++)
    TEST_ASSERT_EQUAL_UINT8(4, sanitize_spoof_pin(bad[i]));
}

void test_ws_config_defaults(void) {
  Bms2Config c;
  TEST_ASSERT_EQUAL_UINT8(4, c.spoof_pin);   // DI1, not the old GPIO21
  TEST_ASSERT_FALSE(c.active_low);           // fixed HIGH=ON in hardware
}

// ---- Waveshare OTA line: version compare with -wsN suffix ----

void test_ws_cmp_within_line(void) {
  TEST_ASSERT_TRUE(ota_cmp_version("2.7-ws2", "2.7-ws1") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.7-ws1", "2.7-ws2") < 0);
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("2.7-ws1", "2.7-ws1"));
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("v2.7-ws1", "2.7-ws1"));
  // Numeric, not lexicographic: ws10 > ws2.
  TEST_ASSERT_TRUE(ota_cmp_version("2.7-ws10", "2.7-ws2") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.7-ws2", "2.7-ws10") < 0);
}

void test_ws_cmp_across_lines_never_newer(void) {
  // Suffixed (Waveshare line) vs plain (generic line): different products,
  // so neither outranks the other — no cross-line auto-update, ever.
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("2.7-ws1", "2.7"));
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("2.7", "2.7-ws1"));
  TEST_ASSERT_EQUAL_INT(0, ota_cmp_version("v2.7-ws1", "v2.7"));
  // Dotted parts still decide when they differ.
  TEST_ASSERT_TRUE(ota_cmp_version("2.8-ws1", "2.7-ws1") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.8", "2.7-ws1") > 0);
  TEST_ASSERT_TRUE(ota_cmp_version("2.7-ws1", "2.8") < 0);
}

void test_ws_tag_matcher(void) {
  TEST_ASSERT_TRUE(ota_tag_is_waveshare_line("v2.7-ws1"));
  TEST_ASSERT_TRUE(ota_tag_is_waveshare_line("v2.10-ws12"));
  TEST_ASSERT_TRUE(ota_tag_is_waveshare_line("V2.7-ws1"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("v2.7"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("v2.7-ws"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("v2.7-wsx"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("v2.7-ws1-evil"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("v2.7-ws1/../x"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line("2.7-ws1"));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line(nullptr));
  TEST_ASSERT_FALSE(ota_tag_is_waveshare_line(""));
}

void test_ws_download_url(void) {
  char url[200];
  TEST_ASSERT_TRUE(ota_download_url("v2.7-ws1", "waveshare", url, sizeof(url)));
  TEST_ASSERT_EQUAL_STRING(
      "https://github.com/bm-a/bms-connection-tester/releases/download/"
      "v2.7-ws1/waveshare-firmware.bin",
      url);
  // Evil tags still rejected.
  TEST_ASSERT_FALSE(ota_download_url("v2.7-ws1/../x", "waveshare", url, sizeof(url)));
  TEST_ASSERT_FALSE(ota_download_url("v2.7-wsx", "waveshare", url, sizeof(url)));
  TEST_ASSERT_FALSE(ota_download_url("2.7-ws1", "waveshare", url, sizeof(url)));
  // Plain tags still work for the generic variants (regression).
  TEST_ASSERT_TRUE(ota_download_url("v2.7", "8mb", url, sizeof(url)));
  TEST_ASSERT_FALSE(ota_download_url("v2.4/../evil", "8mb", url, sizeof(url)));
}

void test_ws_variant_assets(void) {
  TEST_ASSERT_EQUAL_STRING("waveshare-firmware.bin",
                           ota_asset_for_variant("waveshare"));
  TEST_ASSERT_EQUAL_STRING("waveshare-firmware.bin",
                           fw_asset_for_variant("waveshare"));
  // Upload gate: exact basename match per variant (no cross-flash).
  TEST_ASSERT_TRUE(fw_filename_ok("waveshare-firmware.bin", "waveshare"));
  TEST_ASSERT_FALSE(fw_filename_ok("firmware.bin", "waveshare"));
  TEST_ASSERT_FALSE(fw_filename_ok("n16r8-firmware.bin", "waveshare"));
  TEST_ASSERT_FALSE(fw_filename_ok("waveshare-firmware.bin", "8mb"));
  TEST_ASSERT_FALSE(fw_filename_ok("waveshare-firmware.bin", "n16r8"));
}

void run_all() {
  RUN_TEST(test_ws_pin_map);
  RUN_TEST(test_ws_output_byte_all_on_off);
  RUN_TEST(test_ws_output_byte_single_bits);
  RUN_TEST(test_ws_output_byte_count_clipping);
  RUN_TEST(test_ws_output_byte_ignores_active_low);
  RUN_TEST(test_ws_spoof_pin_allowlist);
  RUN_TEST(test_ws_config_defaults);
  RUN_TEST(test_ws_cmp_within_line);
  RUN_TEST(test_ws_cmp_across_lines_never_newer);
  RUN_TEST(test_ws_tag_matcher);
  RUN_TEST(test_ws_download_url);
  RUN_TEST(test_ws_variant_assets);
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
