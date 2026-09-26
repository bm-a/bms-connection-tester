#pragma once
#include <stdint.h>
#include <stdbool.h>

// Waveshare ESP32-S3-ETH-8DI-8RO board map + pure relay-port logic.
// Hardware-independent (no Arduino dependency) so it compiles on the host
// for Unity tests, exactly like bms_protocol.* / relay_ctrl.h.
//
// Map verified 2026-09-26 against:
//  - the official Waveshare wiki pin tables
//    (https://www.waveshare.com/wiki/ESP32-S3-ETH-8DI-8RO)
//  - the Waveshare user manual (ESP32-S3-WROOM-1-N16R8 module: 16 MB flash,
//    8 MB PSRAM; RS485 = isolated SP3485 with *hardware automatic* data
//    direction control — there is NO DE/RE pin)
//  - vendor demo code (szf2020/esp32-s3-eth-8di-8ro-c): TCA9554PWR @ 0x20,
//    EXIO1..8 = output-register bits 0..7, HIGH bit = relay ON,
//    TCA9554PWR_Init(0x00) parks all outputs OFF at boot.
// Digital inputs DI1..DI8 = GPIO4..GPIO11, opto-isolated, active = LOW
// (INPUT_PULLUP), confirmed by the vendor DI example + independent builds.

// ---- TCA9554PWR (relay expander) ----
#define WS_TCA9554_ADDR        0x20
#define WS_TCA9554_REG_OUTPUT  0x01
#define WS_TCA9554_REG_CONFIG  0x03
#define WS_I2C_SDA             42
#define WS_I2C_SCL             41

// ---- RS485: TX17/RX18, hardware auto direction (no DE/RE pin) ----
#define WS_PIN_RS485_TX  17
#define WS_PIN_RS485_RX  18

// ---- Controls: BOOT button = START/STOP; DI terminals for the rest ----
#define WS_PIN_BUTTON     0   // BOOT, press = LOW (strapping pin: holding it
                              // at power-on enters download mode — normal)
#define WS_PIN_DI_BASE    4   // DI1..DI8 = GPIO4..GPIO11, active = LOW
#define WS_PIN_DI_COUNT   8
#define WS_PIN_SPOOF      4   // DI1: spoof trigger (default, web-changeable)
#define WS_PIN_WIFI_KILL  5   // DI2: ground = AP off (default)

// ---- Indicators ----
#define WS_PIN_RGB  38  // onboard WS2812 (discrete green/red LEDs don't exist)

// ---- Reserved: do not touch ----
 // GPIO12..16 = W5500 Ethernet (INT/MOSI/MISO/SCLK/CS)
 // GPIO40     = RTC interrupt; GPIO41/42 shared with RTC @ 0x51
 // GPIO46     = buzzer (unused by this firmware)

// Logical relay i (0..7) -> EXIO(i+1) -> output-register bit i.
// Vendor demo: HIGH bit = relay ON (Relay_Open = Set_EXIO(CH, true)).
static inline uint8_t ws_relay_bit(uint8_t i) { return (uint8_t)(1u << i); }

// Whole-port output byte for logical relay states with relay-count clipping:
// only the first relay_count relays participate; the rest are forced OFF
// (same rule as the direct-GPIO path in main.cpp). The TCA9554 output stage
// is fixed HIGH-bit = ON, so cfg.active_low is intentionally NOT applied
// here — dashboard labels always match the hardware on this board.
static inline uint8_t ws_output_byte(const bool on[8], uint8_t relay_count) {
  uint8_t n = relay_count > 8 ? 8 : relay_count;
  uint8_t b = 0;
  for (uint8_t i = 0; i < n; i++)
    if (on[i]) b |= ws_relay_bit(i);
  return b;
}
