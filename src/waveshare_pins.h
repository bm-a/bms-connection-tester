#pragma once
#include <stdint.h>
#include <stdbool.h>

// Waveshare ESP32-S3-POE-ETH-8DI-8DO board map + pure port logic.
// Hardware-independent (no Arduino dependency) so it compiles on the host
// for Unity tests, exactly like bms_protocol.* / relay_ctrl.h.
//
// Map verified 2026-09-27 against the OFFICIAL 8DO demo
// (ESP32-S3-POE-ETH-8DI-8DO-Demo.zip from
//  https://www.waveshare.com/wiki/ESP32-S3-POE-ETH-8DI-8DO):
//  - WS_GPIO.h: TXD1=17, RXD1=18, TXD1EN=21, GPIO_PIN_RGB=38,
//    GPIO_PIN_Buzzer=46; WS_DIN.h: DIN_PIN_CH1..8 = GPIO4..GPIO11.
//  - WS_RS485.cpp RS485_Init(): lidarSerial.begin(9600, SERIAL_8N1, RXD1,
//    TXD1) then setPins(-1,-1,-1,TXD1EN) + setMode(UART_MODE_RS485_HALF_DUPLEX)
//    — the ESP32 UART peripheral auto-drives GPIO21 (RTS) for direction.
//    (The wiki product page's "hardware-flow direction" = this UART mode,
//    not board hardware: without the UART mode or manual DE the ESP32
//    receives nothing — proven 2026-09-27.)
//  - WS_TCA9554PWR.h/.cpp: TCA9554PWR @ 0x20, OUTPUT reg 0x01, CONFIG reg
//    0x03; Set_EXIO(CHx,true) sets bit (CHx-1) -> EXIOx; Dout_Init() calls
//    TCA9554PWR_Init(0x00, 0xFF) = write OUTPUT_REG first, then CONFIG_REG.
//    HIGH bit = channel ON (Darlington sink outputs, 500 mA).
//  - WS_GPIO.cpp RGB_Light(r,g,b) -> neopixelWrite(pin, g, r, b): the RGB
//    element expects RGB byte order, so R/G are swapped on the wire.
//  - WS_DIN.cpp DIN_Init(): pinMode(DIN_PIN_CHx, INPUT_PULLUP) — digital
//    inputs are opto-isolated, active = LOW.
// I2C: SDA=42/SCL=41 per demo I2C_Driver.h (shared with RTC @ 0x51).

// ---- TCA9554PWR (DO expander) ----
#define WS_TCA9554_ADDR        0x20
#define WS_TCA9554_REG_OUTPUT  0x01
#define WS_TCA9554_REG_CONFIG  0x03
#define WS_I2C_SDA             42
#define WS_I2C_SCL             41

// TCA9554 boot sequence, mirroring the official 8DO demo
// (WS_Dout.cpp Dout_Init -> TCA9554PWR_Init(PinMode, PinState)):
// the OUTPUT register is written BEFORE the CONFIG register.
// The demo passes PinState=0xFF (all 8 channels ON at boot); this firmware
// deliberately parks WS_TCA9554_BOOT_OUTPUT=0x00 (all OFF) instead — the
// safe boot state for an automated test bench. The register ORDER is the
// part that must match the demo; the park value is our application choice.
#define WS_TCA9554_BOOT_OUTPUT 0x00
#define WS_TCA9554_BOOT_CONFIG 0x00
struct WsTcaInitStep { uint8_t reg; uint8_t val; };
static inline WsTcaInitStep ws_tca_init_step(uint8_t step) {
  WsTcaInitStep s = {0, 0};
  if (step == 0)      { s.reg = WS_TCA9554_REG_OUTPUT; s.val = WS_TCA9554_BOOT_OUTPUT; }
  else if (step == 1) { s.reg = WS_TCA9554_REG_CONFIG; s.val = WS_TCA9554_BOOT_CONFIG; }
  return s;
}
#define WS_TCA9554_INIT_STEPS 2

// ---- RS485: TX17/RX18; direction on GPIO21 (RTS/TXD1EN) ----
// Official 8DO demo (WS_RS485.cpp): UART_MODE_RS485_HALF_DUPLEX via
// setPins(-1,-1,-1,TXD1EN) + setMode() — the ESP32 UART peripheral
// auto-drives GPIO21 (HIGH=TX, LOW=RX). main.cpp uses that mode and keeps
// manual digitalWrite(DE) only as a fallback if setMode() fails.
#define WS_PIN_RS485_TX  17
#define WS_PIN_RS485_RX  18
#define WS_PIN_RS485_DE  21

// ---- Controls: BOOT button = START/STOP; DI terminals for the rest ----
#define WS_PIN_BUTTON     0   // BOOT, press = LOW (strapping pin: holding it
                              // at power-on enters download mode — normal)
#define WS_PIN_DI_BASE    4   // DI1..DI8 = GPIO4..GPIO11, active = LOW (NPN opto pulls LOW)
#define WS_PIN_DI_COUNT   8
#define WS_PIN_SPOOF      4   // DI1: spoof trigger (default, web-changeable)
#define WS_PIN_WIFI_KILL  5   // DI2: ground = AP off (default)
#define WS_PIN_RELAY_TRIGGER 6 // DI3: trigger relay sequencer (START/STOP)

// ---- Indicators ----
#define WS_PIN_RGB  38  // onboard WS2812 (discrete green/red LEDs don't exist)

// ---- Buzzer (GPIO46) ----
#define WS_PIN_BUZZER   46  // active buzzer, LEDC 1 kHz / 8-bit, duty <= 200
#define WS_BUZZER_DUTY  200 // official demo Dutyfactor (of 255 max)
// Strapping pin: the ESP glue must never drive it before setup() (the
// official demo's GPIO_Init runs in setup too); ledcWrite(0) parks silent.

// ---- Reserved: do not touch ----
 // GPIO12..16 = W5500 Ethernet (INT/MOSI/MISO/SCLK/CS) — driven by ws_eth.cpp
 // GPIO40     = RTC interrupt; GPIO41/42 shared with RTC @ 0x51

// Logical output i (0..7) -> EXIO(i+1) -> output-register bit i.
// Official demo: HIGH bit = channel ON (Dout_Open = Set_EXIO(CH, true);
// ALL_ON writes 0xFF). The 8DO channels are Darlington sink outputs
// (500 mA), not relay coils, but the EXIO interface is identical.
static inline uint8_t ws_relay_bit(uint8_t i) { return (uint8_t)(1u << i); }

// Whole-port output byte for logical channel states with count clipping:
// only the first relay_count channels participate; the rest are forced OFF
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

// ---- Buzzer pattern engine — pure logic, host-tested ----
// Models the official 8DO demo's Buzzer_Open_Time() queue
// (Buzzer_Indicate[10] drained by BuzzerTask) without tasks or delays: the
// ESP glue advances tick(now) from loop() and drives LEDC only when `out`
// changes. Frequency/duty (1 kHz, 8-bit, duty WS_BUZZER_DUTY) live in the
// ESP glue (main.cpp); this engine only computes the on/off pattern.
#define WS_BUZZER_QUEUE 4
struct WsBuzzer {
  struct Req { uint16_t total_ms; uint16_t flick_ms; };
  Req q[WS_BUZZER_QUEUE];
  uint8_t qn = 0;
  bool run = false;
  Req cur = {0, 0};
  unsigned long start_ms = 0;
  bool out = false;  // current output level; ESP writes duty when it changes
  // Queue a beep: total_ms long, toggling every flick_ms (0 = solid on).
  // Sub-50 ms flicker counts as solid, like the demo. Drops when full.
  bool push(uint16_t total_ms, uint16_t flick_ms) {
    if (total_ms == 0 || qn >= WS_BUZZER_QUEUE) return false;
    if (flick_ms < 51) flick_ms = 0;
    q[qn++] = {total_ms, flick_ms};
    return true;
  }
  // Advance to `now` (millis). Returns true exactly when `out` changed.
  bool tick(unsigned long now) {
    if (!run) {
      if (qn == 0) return false;
      cur = q[0];
      for (uint8_t i = 1; i < qn; i++) q[i - 1] = q[i];
      qn--;
      run = true;
      start_ms = now;
      out = false;
    }
    unsigned long el = now - start_ms;
    if (el >= cur.total_ms) {
      run = false;
      if (out) { out = false; return true; }
      return false;
    }
    bool want = true;
    if (cur.flick_ms) want = ((el / cur.flick_ms) % 2) == 0;
    if (want != out) { out = want; return true; }
    return false;
  }
  bool idle() const { return !run && qn == 0; }
};

// Buzzer output gate (mirrors main.cpp buzzer_beep): a beep only reaches the
// WsBuzzer queue when the LEDC channel is up (ready) AND the user enabled the
// buzzer in the web UI (default OFF — silent until explicitly enabled).
// Host-testable: the firmware's buzzer_beep() must call this.
static inline bool ws_buzzer_gate(bool ready, bool enabled) {
  return ready && enabled;
}

// ---- TCA9554 fault latch — pure logic, host-tested ----
// Live expander status for /api/state ("expander":"ok|fail"), the RGB fault
// override and the one-shot buzzer alarm. Unlike the demo's DoutFailTask
// (which clears Failure_Flag after each 5 s alarm), this is a live status:
// false on any bus error, true again on any success (self-heals when the
// bus recovers). note() returns true exactly on the true->false edge so the
// alarm fires once per outage, not on every eval while it persists.
struct WsTcaFault {
  bool ok = true;
  bool note(bool write_ok) {
    if (write_ok) { ok = true; return false; }
    if (ok) { ok = false; return true; }
    return false;
  }
};
