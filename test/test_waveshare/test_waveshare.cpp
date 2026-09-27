#include <unity.h>
#include "../../src/waveshare_pins.h"
#include "../../src/ws_eth.h"
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
  TEST_ASSERT_EQUAL_UINT8(46, WS_PIN_BUZZER);
}

// ---- W5500 Ethernet pinout (official 8DO demo WS_ETH.h) ----

void test_ws_8do_demo_eth_pins(void) {
  // Demo: SPI.begin(15, 14, 13);
  // ETH.begin(ETH_PHY_W5500, /*addr*/ 1, /*cs*/ 16, /*irq*/ 12, /*rst*/ 39, SPI)
  TEST_ASSERT_EQUAL_UINT8(15, WS_ETH_SPI_SCK);
  TEST_ASSERT_EQUAL_UINT8(14, WS_ETH_SPI_MISO);
  TEST_ASSERT_EQUAL_UINT8(13, WS_ETH_SPI_MOSI);
  TEST_ASSERT_EQUAL_UINT8(16, WS_ETH_CS);
  TEST_ASSERT_EQUAL_UINT8(12, WS_ETH_IRQ);
  TEST_ASSERT_EQUAL_UINT8(39, WS_ETH_RST);
  TEST_ASSERT_EQUAL_UINT8(1, WS_ETH_PHY_ADDR);
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

// ---- official ESP32-S3-POE-ETH-8DI-8DO demo alignment ----
// Values below are taken verbatim from the Waveshare demo
// (ESP32-S3-POE-ETH-8DI-8DO-Demo.zip):
//   WS_GPIO.h: TXD1=17, RXD1=18, TXD1EN=21, GPIO_PIN_RGB=38
//   WS_DIN.h:  DIN_PIN_CH1..8 = 4..11
//   WS_TCA9554PWR.h/.cpp: addr 0x20, OUTPUT reg 0x01, CONFIG reg 0x03,
//     Set_EXIO(CHx,1) sets bit (CHx-1); Dout_Init = TCA9554PWR_Init(0x00,0xFF)
//     i.e. OUTPUT_REG written BEFORE CONFIG_REG.
//   WS_RS485.cpp RS485_Init: setPins(-1,-1,-1,TXD1EN) +
//     setMode(UART_MODE_RS485_HALF_DUPLEX) — GPIO21 is the UART RTS pin.

void test_ws_8do_demo_rs485_pins(void) {
  TEST_ASSERT_EQUAL_UINT8(17, WS_PIN_RS485_TX);  // demo TXD1
  TEST_ASSERT_EQUAL_UINT8(18, WS_PIN_RS485_RX);  // demo RXD1
  TEST_ASSERT_EQUAL_UINT8(21, WS_PIN_RS485_DE);  // demo TXD1EN (RTS)
}

void test_ws_8do_demo_tca_regs(void) {
  TEST_ASSERT_EQUAL_UINT8(0x20, WS_TCA9554_ADDR);       // demo TCA9554_ADDRESS
  TEST_ASSERT_EQUAL_UINT8(0x01, WS_TCA9554_REG_OUTPUT); // demo TCA9554_OUTPUT_REG
  TEST_ASSERT_EQUAL_UINT8(0x03, WS_TCA9554_REG_CONFIG); // demo TCA9554_CONFIG_REG
  TEST_ASSERT_EQUAL_UINT8(42, WS_I2C_SDA);              // demo I2C_SDA_PIN
  TEST_ASSERT_EQUAL_UINT8(41, WS_I2C_SCL);              // demo I2C_SCL_PIN
}

void test_ws_8do_demo_tca_init_sequence(void) {
  // Demo order: OUTPUT_REG first, then CONFIG_REG (TCA9554PWR_Init body:
  // Set_EXIOS(PinState) then Mode_EXIOS(PinMode)). The firmware's
  // tca_relays_init() walks this exact table, so this test covers the
  // shipped init order.
  TEST_ASSERT_EQUAL_UINT8(2, WS_TCA9554_INIT_STEPS);
  WsTcaInitStep s0 = ws_tca_init_step(0);
  WsTcaInitStep s1 = ws_tca_init_step(1);
  TEST_ASSERT_EQUAL_UINT8(WS_TCA9554_REG_OUTPUT, s0.reg);
  TEST_ASSERT_EQUAL_UINT8(WS_TCA9554_REG_CONFIG, s1.reg);
  // Demo parks PinState=0xFF (all channels ON); we deliberately park OFF
  // (0x00) — safe boot state for a test bench. CONFIG is all-outputs (0x00)
  // in both.
  TEST_ASSERT_EQUAL_UINT8(0x00, WS_TCA9554_BOOT_OUTPUT);
  TEST_ASSERT_EQUAL_UINT8(0x00, WS_TCA9554_BOOT_CONFIG);
  TEST_ASSERT_EQUAL_UINT8(WS_TCA9554_BOOT_OUTPUT, s0.val);
  TEST_ASSERT_EQUAL_UINT8(WS_TCA9554_BOOT_CONFIG, s1.val);
}

void test_ws_8do_demo_dout_polarity(void) {
  // Demo Dout_Open(CHx) -> Set_EXIO(CHx, true) sets bit (CHx-1); ALL_ON
  // writes 0xFF. HIGH bit = channel ON (Darlington sink outputs).
  for (uint8_t ch = 1; ch <= 8; ch++)
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(1u << (ch - 1)), ws_relay_bit(ch - 1));
  const bool all_on[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  TEST_ASSERT_EQUAL_UINT8(0xFF, ws_output_byte(all_on, 8));  // demo ALL_ON
  const bool all_off[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8(0x00, ws_output_byte(all_off, 8)); // demo ALL_OFF
}

// ---- Buzzer (GPIO46): 1 kHz / 8-bit / duty <= 200 (official demo) ----

void test_ws_buzzer_constants(void) {
  TEST_ASSERT_EQUAL_UINT8(200, WS_BUZZER_DUTY);  // demo Dutyfactor (of 255)
  TEST_ASSERT_TRUE(WS_BUZZER_DUTY <= 200);       // hard cap, never exceed demo
}

void test_ws_buzzer_solid_beep(void) {
  WsBuzzer b;
  TEST_ASSERT_TRUE(b.idle());
  TEST_ASSERT_TRUE(b.push(200, 0));  // 200 ms solid (link transition beep)
  TEST_ASSERT_TRUE(b.tick(0));       // starts: out 0->1
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_FALSE(b.tick(100));    // mid-beep: no change
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_TRUE(b.tick(200));     // expires: out 1->0
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_TRUE(b.idle());
  TEST_ASSERT_FALSE(b.tick(300));    // idle: silent
}

void test_ws_buzzer_flicker(void) {
  WsBuzzer b;
  TEST_ASSERT_TRUE(b.push(500, 150));  // 500 ms, toggles every 150 ms
  TEST_ASSERT_TRUE(b.tick(0));
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_TRUE(b.tick(150));   // 1->0
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_TRUE(b.tick(300));   // 0->1
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_TRUE(b.tick(450));   // 1->0
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_FALSE(b.tick(500));  // expiry while already 0: no change
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_TRUE(b.idle());
}

void test_ws_buzzer_sub50_flicker_is_solid(void) {
  // Demo rule (Buzzer_Open_Time): flicker < 50 ms counts as solid on.
  WsBuzzer b;
  TEST_ASSERT_TRUE(b.push(200, 30));
  TEST_ASSERT_TRUE(b.tick(0));
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_FALSE(b.tick(60));
  TEST_ASSERT_FALSE(b.tick(120));
  TEST_ASSERT_TRUE(b.out);  // never toggled
  TEST_ASSERT_TRUE(b.tick(200));
  TEST_ASSERT_FALSE(b.out);
}

void test_ws_buzzer_fifo_and_full(void) {
  WsBuzzer b;
  TEST_ASSERT_TRUE(b.push(100, 0));
  TEST_ASSERT_TRUE(b.push(200, 0));
  TEST_ASSERT_TRUE(b.push(300, 0));
  TEST_ASSERT_TRUE(b.push(400, 0));
  TEST_ASSERT_FALSE(b.push(500, 0));  // queue full: dropped, never blocks
  TEST_ASSERT_TRUE(b.tick(0));    // first request starts
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_TRUE(b.tick(100));  // first expires -> out 1->0
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_TRUE(b.tick(100));  // second starts immediately after
  TEST_ASSERT_TRUE(b.out);
  TEST_ASSERT_TRUE(b.tick(300));  // second expires (100 + 200)
  TEST_ASSERT_FALSE(b.out);
  TEST_ASSERT_TRUE(b.tick(300));  // third starts
  TEST_ASSERT_TRUE(b.out);
}

void test_ws_buzzer_empty_push_rejected(void) {  WsBuzzer b;
  TEST_ASSERT_FALSE(b.push(0, 0));
  TEST_ASSERT_TRUE(b.idle());
}

void test_ws_buzzer_gate_disabled_is_silent(void) {
  // The firmware's buzzer_beep() gates on ws_buzzer_gate(ready, enabled):
  // disabled == silent, even when the LEDC channel is up.
  TEST_ASSERT_FALSE(ws_buzzer_gate(false, false));
  TEST_ASSERT_FALSE(ws_buzzer_gate(true, false));   // default: silent
  TEST_ASSERT_FALSE(ws_buzzer_gate(false, true));   // not ready: silent
  TEST_ASSERT_TRUE(ws_buzzer_gate(true, true));     // enabled: sounds
  // End-to-end at the queue level: a gated-out beep never reaches WsBuzzer.
  WsBuzzer b;
  bool ready = true, enabled = false;  // user toggle OFF
  if (ws_buzzer_gate(ready, enabled)) b.push(200, 0);
  TEST_ASSERT_TRUE(b.idle());
  TEST_ASSERT_FALSE(b.tick(0));
  enabled = true;  // user enables the toggle
  if (ws_buzzer_gate(ready, enabled)) b.push(200, 0);
  TEST_ASSERT_FALSE(b.idle());
  TEST_ASSERT_TRUE(b.tick(0));
  TEST_ASSERT_TRUE(b.out);
}

// ---- TCA9554 fault latch (one-shot alarm edge, self-heal) ----

void test_ws_tca_fault_edge_and_selfheal(void) {
  WsTcaFault f;
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_FALSE(f.note(true));   // success: no edge, stays ok
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_TRUE(f.note(false));   // first failure: edge fires (alarm)
  TEST_ASSERT_FALSE(f.ok);
  TEST_ASSERT_FALSE(f.note(false));  // still failed: no repeat edge
  TEST_ASSERT_FALSE(f.ok);
  TEST_ASSERT_FALSE(f.note(true));   // bus recovers: self-heals, no edge
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_TRUE(f.note(false));   // new outage: edge fires again
  TEST_ASSERT_FALSE(f.ok);
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
  RUN_TEST(test_ws_8do_demo_rs485_pins);
  RUN_TEST(test_ws_8do_demo_tca_regs);
  RUN_TEST(test_ws_8do_demo_tca_init_sequence);
  RUN_TEST(test_ws_8do_demo_dout_polarity);
  RUN_TEST(test_ws_8do_demo_eth_pins);
  RUN_TEST(test_ws_buzzer_constants);
  RUN_TEST(test_ws_buzzer_solid_beep);
  RUN_TEST(test_ws_buzzer_flicker);
  RUN_TEST(test_ws_buzzer_sub50_flicker_is_solid);
  RUN_TEST(test_ws_buzzer_fifo_and_full);
  RUN_TEST(test_ws_buzzer_empty_push_rejected);
  RUN_TEST(test_ws_buzzer_gate_disabled_is_silent);
  RUN_TEST(test_ws_tca_fault_edge_and_selfheal);
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
