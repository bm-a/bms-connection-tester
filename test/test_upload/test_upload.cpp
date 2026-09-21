#include <unity.h>
#include "../../src/fw_upload.h"

void setUp(void) {}
void tearDown(void) {}

// v2.4 Tasmota-grade update gates: explicit size budget, exact variant asset
// match, 0xE9 magic + flash-size-vs-chip. Every failure names its cause.

void test_max_space_pages(void) {
  // Tasmota: (free - 0x1000) & ~0xFFF. 8 MB free sketch example.
  TEST_ASSERT_EQUAL_UINT32(0x7FF000u, fw_max_sketch_space(0x800000u));
  TEST_ASSERT_EQUAL_UINT32(0, fw_max_sketch_space(0x1000u));
  TEST_ASSERT_EQUAL_UINT32(0, fw_max_sketch_space(0));
  // 950 KB firmware fits an 8 MB sketch budget comfortably.
  TEST_ASSERT_TRUE(fw_size_ok(950000, fw_max_sketch_space(0x800000u)));
  TEST_ASSERT_FALSE(fw_size_ok(0x800000u, fw_max_sketch_space(0x800000u)));
  TEST_ASSERT_TRUE(fw_size_ok(0, 0x1000u));  // unknown length streams
  TEST_ASSERT_FALSE(fw_size_ok(100, 0));     // no budget at all
}

void test_asset_names(void) {
  TEST_ASSERT_EQUAL_STRING("firmware.bin", fw_asset_for_variant("8mb"));
  TEST_ASSERT_EQUAL_STRING("n16r8-firmware.bin", fw_asset_for_variant("n16r8"));
  TEST_ASSERT_EQUAL_STRING("firmware.bin", fw_asset_for_variant(nullptr));
}

void test_filename_suffix_match(void) {
  // Exact asset (browser may send a bare name or a path).
  TEST_ASSERT_TRUE(fw_filename_ok("firmware.bin", "8mb"));
  TEST_ASSERT_TRUE(fw_filename_ok("n16r8-firmware.bin", "n16r8"));
  // Substring trap: the 8 MB asset name sits INSIDE the n16r8 name —
  // suffix matching must still tell them apart (no cross-flash).
  TEST_ASSERT_FALSE(fw_filename_ok("n16r8-firmware.bin", "8mb"));
  TEST_ASSERT_FALSE(fw_filename_ok("firmware.bin", "n16r8"));
  // Wrong files, empty names, case-insensitive filesystems.
  TEST_ASSERT_FALSE(fw_filename_ok("tasmota.bin", "8mb"));
  TEST_ASSERT_FALSE(fw_filename_ok("", "8mb"));
  TEST_ASSERT_FALSE(fw_filename_ok(nullptr, "8mb"));
  TEST_ASSERT_TRUE(fw_filename_ok("FIRMWARE.BIN", "8mb"));
  TEST_ASSERT_TRUE(fw_filename_ok("C:\\dl\\n16r8-firmware.bin", "n16r8"));
}

void test_image_head_gate(void) {
  // 8 MB image (code 3) on an 8 MB chip: OK.
  const uint8_t img8[] = {0xE9, 0x06, 0x02, 0x30};
  TEST_ASSERT_EQUAL_INT(FW_OK, fw_gate_image_head(img8, 4, 0x800000u));
  // 16 MB image (code 4) on an 8 MB chip: rejected before any flash write.
  const uint8_t img16[] = {0xE9, 0x06, 0x02, 0x4F};
  TEST_ASSERT_EQUAL_INT(FW_FLASH_TOO_BIG,
                        fw_gate_image_head(img16, 4, 0x800000u));
  // 16 MB image on a 16 MB chip: OK.
  TEST_ASSERT_EQUAL_INT(FW_OK, fw_gate_image_head(img16, 4, 0x1000000u));
  // Not firmware at all.
  const uint8_t elf[] = {0x7F, 'E', 'L', 'F'};
  TEST_ASSERT_EQUAL_INT(FW_BAD_MAGIC, fw_gate_image_head(elf, 4, 0x800000u));
  const uint8_t cfg[] = {'{', '"', 'r', 'm'};
  TEST_ASSERT_EQUAL_INT(FW_BAD_MAGIC, fw_gate_image_head(cfg, 4, 0x800000u));
  // Truncated head: not committable.
  TEST_ASSERT_EQUAL_INT(FW_BAD_MAGIC, fw_gate_image_head(img8, 3, 0x800000u));
  TEST_ASSERT_EQUAL_INT(FW_BAD_MAGIC, fw_gate_image_head(nullptr, 0, 8u << 20));
  // Bogus size code.
  const uint8_t bad[] = {0xE9, 0x06, 0x02, 0xF0};
  TEST_ASSERT_EQUAL_INT(FW_FLASH_TOO_BIG,
                        fw_gate_image_head(bad, 4, 0x8000000u));
}

void test_err_strings_cover_all(void) {
  // Every failure names its cause on the page — no bare 403s, no dead ends.
  TEST_ASSERT_EQUAL_STRING("ok", fw_err_str(FW_OK));
  for (int e = (int)FW_BAD_PASS; e <= (int)FW_BEGIN_FAIL; e++) {
    const char *s = fw_err_str((FwUploadErr)e);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s[0] != '\0');
  }
}

void run_all() {
  RUN_TEST(test_max_space_pages);
  RUN_TEST(test_asset_names);
  RUN_TEST(test_filename_suffix_match);
  RUN_TEST(test_image_head_gate);
  RUN_TEST(test_err_strings_cover_all);
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
