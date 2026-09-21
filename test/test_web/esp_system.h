// Host stub: esp_system.h reset reason (test_web ONLY). Test-driven reason
// code; the ESP_RST_* values match ESP-IDF's esp_reset_reason_t. Never in
// firmware (real Arduino core provides <esp_system.h>).
#pragma once

enum {
  ESP_RST_UNKNOWN = 0,
  ESP_RST_POWERON = 1,
  ESP_RST_EXT = 2,
  ESP_RST_SW = 3,
  ESP_RST_PANIC = 4,
  ESP_RST_INT_WDT = 5,
  ESP_RST_TASK_WDT = 6,
  ESP_RST_WDT = 7,
  ESP_RST_DEEPSLEEP = 8,
  ESP_RST_BROWNOUT = 9,
  ESP_RST_SDIO = 10,
};
typedef int esp_reset_reason_t;

inline int g_esp_reset_reason_code = ESP_RST_POWERON;
inline esp_reset_reason_t esp_reset_reason() {
  return (esp_reset_reason_t)g_esp_reset_reason_code;
}
