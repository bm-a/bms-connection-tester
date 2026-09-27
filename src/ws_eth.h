#pragma once
// Waveshare ESP32-S3-POE-ETH-8DI-8DO: W5500 Ethernet.
//
// Our Arduino core here is 2.0.17, whose Arduino-ETH library has NO W5500
// support (that arrived with Arduino-ESP32 >= 3.0, which the official 8DO
// demo WS_ETH.cpp targets). So this module drives the ESP-IDF esp_eth
// W5500 MAC+PHY directly and attaches a real lwIP netif: the existing
// WebServer (bound to all interfaces) then answers on the Ethernet IP
// with zero web-code changes. DHCP client + hostname "bms-tester",
// same observable behavior as the demo (ETH_START -> hostname,
// ETH_GOT_IP -> print IP).
//
// Best-effort by design: every step is checked and any failure just skips
// Ethernet — boot and WiFi (AP 192.168.4.1, STA, captive portal) are never
// affected.

// Official demo pinout (WS_ETH.h): SPI.begin(15, 14, 13),
// ETH.begin(ETH_PHY_W5500, /*addr*/ 1, /*cs*/ 16, /*irq*/ 12, /*rst*/ 39, SPI)
#define WS_ETH_SPI_SCK  15
#define WS_ETH_SPI_MISO 14
#define WS_ETH_SPI_MOSI 13
#define WS_ETH_CS       16
#define WS_ETH_IRQ      12
#define WS_ETH_RST      39
#define WS_ETH_PHY_ADDR 1

#ifdef ARDUINO
void ws_eth_init();            // call once from setup(), Waveshare build only
bool ws_eth_up();              // link up with an address
const char *ws_eth_ip_str();   // dotted-quad when up, "" otherwise
#else
// Host stubs (unit-test builds; the ESP implementation is never host-built).
inline void ws_eth_init() {}
inline bool ws_eth_up() { return false; }
inline const char *ws_eth_ip_str() { return ""; }
#endif
