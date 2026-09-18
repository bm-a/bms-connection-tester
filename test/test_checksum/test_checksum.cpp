#include <unity.h>
#include <cstring>
#include "../../src/bms_protocol.h"

static const uint8_t GOLDEN_100[] = {
  0xDD, 0x03, 0x00, 0x1B, 0x14, 0x50, 0x00, 0x00, 0x27, 0x10, 0x27, 0x10,
  0x00, 0x01, 0x20, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x64,
  0x03, 0x0E, 0x02, 0x0B, 0xA5, 0x0B, 0xA5, 0xFC, 0xDA, 0x77
};

void setUp(void) {}
void tearDown(void) {}

// Request cover = CMD+LEN (byte1 A5 excluded by slice choice)
void test_request_checksum_fffd(void) {
  uint8_t cover[] = {0x03, 0x00};
  TEST_ASSERT_EQUAL_HEX16(0xFFFD, jbd_checksum(cover, 2));
}

// Response cover = STATUS+LEN+DATA = packet[2:-3] (echoed cmd excluded)
void test_soc100_checksum_fcda(void) {
  // BMS_RESPONSE[2..30] = len_hi,len_lo + 27 data bytes
  TEST_ASSERT_EQUAL_HEX16(0xFCDA, jbd_checksum(BMS_RESPONSE + 2, 29));
}

// Regression: SOC 50% vector, same cover rule -> FCA8
void test_soc50_checksum_fca8(void) {
  uint8_t frame[] = {
    0xDD, 0x03, 0x00, 0x1B, 0x14, 0x50, 0x00, 0x00, 0x13, 0x88, 0x27, 0x10,
    0x00, 0x01, 0x20, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x32,
    0x03, 0x0E, 0x02, 0x0B, 0xA5, 0x0B, 0xA5, 0xFC, 0xA8, 0x77
  };
  TEST_ASSERT_EQUAL_HEX16(0xFCA8, jbd_checksum(frame + 2, 29));
}

// Regression: 90%/45C vector (0x22 len) -> FA86
void test_90pct_checksum_fa86(void) {
  uint8_t frame[] = {
    0xDD, 0x03, 0x00, 0x22, 0x14, 0xD7, 0x00, 0x00, 0x24, 0xC1, 0x29, 0x04,
    0x00, 0x3E, 0x33, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD9, 0x5A,
    0x03, 0x10, 0x01, 0x0C, 0x6E, 0x00, 0x00, 0x00, 0x29, 0x04, 0x24, 0xC1,
    0x00, 0x00, 0xFA, 0x86, 0x77
  };
  TEST_ASSERT_EQUAL_HEX16(0xFA86, jbd_checksum(frame + 2, 36));
}

// Golden frame byte-exact: compiled response must equal capture.
void test_golden_frame_exact(void) {
  TEST_ASSERT_EQUAL_UINT8_ARRAY(GOLDEN_100, BMS_RESPONSE, BMS_RESPONSE_LEN);
  TEST_ASSERT_EQUAL_HEX8(0xDD, BMS_RESPONSE[0]);
  TEST_ASSERT_EQUAL_HEX8(0x77, BMS_RESPONSE[BMS_RESPONSE_LEN - 1]);
}

// Matcher: exact passes, 1-byte corruption fails.
void test_matcher_exact_true_corrupt_false(void) {
  uint8_t good[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
  TEST_ASSERT_TRUE(matches_request(good));
  for (int i = 0; i < 7; i++) {
    uint8_t bad[7];
    memcpy(bad, good, 7);
    bad[i] ^= 0x01;
    TEST_ASSERT_FALSE(matches_request(bad));
  }
}

void run_all() {
  RUN_TEST(test_request_checksum_fffd);
  RUN_TEST(test_soc100_checksum_fcda);
  RUN_TEST(test_soc50_checksum_fca8);
  RUN_TEST(test_90pct_checksum_fa86);
  RUN_TEST(test_golden_frame_exact);
  RUN_TEST(test_matcher_exact_true_corrupt_false);
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
