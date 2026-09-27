// W5500 Ethernet for the Waveshare ESP32-S3-POE-ETH-8DI-8DO (ESP-only).
//
// Why raw ESP-IDF esp_eth instead of Arduino ETH.begin(ETH_PHY_W5500,...):
// this build uses Arduino-ESP32 2.0.17, whose ETH.h is the old RMII-only
// API — there is no ETH_PHY_W5500 symbol and no six-argument SPI begin.
// The ESP-IDF 4.4 underneath ships the W5500 MAC/PHY drivers
// (CONFIG_ETH_SPI_ETHERNET_W5500 is set), so we bind them directly. When
// the build one day moves to Arduino-ESP32 3.x, this module can be retired
// in favor of the demo's ETH.begin() call.
//
// Every init step is fallible and checked: any failure returns early and
// leaves Ethernet disabled. Boot, the AP, STA, the captive portal and OTA
// policy are untouched. DHCP supplies the address; on GOT_IP we record it
// for /api/state ("eth_ip") and the dashboard.
//
// NOTE on routing: with WiFi STA also up, outbound traffic (OTA check and
// download, ntp-less time sync) follows lwIP's default interface, which is
// the most recently connected one. The STA-online gate for OTA checks
// still applies exactly as before — Ethernet is an extra management path,
// not an OTA-policy change.
#ifdef ARDUINO

#include "ws_eth.h"

#include <Arduino.h>

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

static bool s_eth_up = false;
static char s_eth_ip[16] = "";  // "xxx.xxx.xxx.xxx" or ""

static void ws_eth_handler(void *arg, esp_event_base_t base, int32_t id,
                           void *data) {
  (void)arg;
  if (base == ETH_EVENT) {
    if (id == ETHERNET_EVENT_DISCONNECTED || id == ETHERNET_EVENT_STOP) {
      s_eth_up = false;
      s_eth_ip[0] = '\0';
    }
  } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
    snprintf(s_eth_ip, sizeof(s_eth_ip), IPSTR, IP2STR(&e->ip_info.ip));
    s_eth_up = true;
  }
}

void ws_eth_init() {
  // 1. SPI bus for the W5500 (SPI2/FSPI; Arduino SPI is never begun here).
  spi_bus_config_t bus = {};
  bus.mosi_io_num = WS_ETH_SPI_MOSI;
  bus.miso_io_num = WS_ETH_SPI_MISO;
  bus.sclk_io_num = WS_ETH_SPI_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return;

  spi_device_interface_config_t dev = {};
  dev.command_bits = 16;  // W5500 frame: 16-bit offset + 8-bit control
  dev.address_bits = 8;
  dev.mode = 0;
  dev.clock_speed_hz = SPI_MASTER_FREQ_20M;
  dev.spics_io_num = WS_ETH_CS;
  dev.queue_size = 20;
  spi_device_handle_t spi_hdl = nullptr;
  if (spi_bus_add_device(SPI2_HOST, &dev, &spi_hdl) != ESP_OK) return;

  // 2. W5500 MAC + PHY.
  eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(spi_hdl);
  w5500.int_gpio_num = WS_ETH_IRQ;
  eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
  esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
  if (!mac) return;

  eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
  phy_cfg.phy_addr = WS_ETH_PHY_ADDR;
  phy_cfg.reset_gpio_num = WS_ETH_RST;
  esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);
  if (!phy) {
    mac->del(mac);
    return;
  }

  esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
  esp_eth_handle_t eth_hdl = nullptr;
  if (esp_eth_driver_install(&eth_cfg, &eth_hdl) != ESP_OK) {
    phy->del(phy);
    mac->del(mac);
    return;
  }

  // 3. Unique MAC for the Ethernet interface (locally administered).
  uint8_t mac_addr[6];
  if (esp_read_mac(mac_addr, ESP_MAC_ETH) == ESP_OK)
    esp_eth_ioctl(eth_hdl, ETH_CMD_S_MAC_ADDR, mac_addr);

  // 4. lwIP netif + glue, hostname, DHCP client.
  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
  if (!eth_netif) {
    esp_eth_driver_uninstall(eth_hdl);
    return;
  }
  if (esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_hdl)) != ESP_OK) {
    esp_eth_driver_uninstall(eth_hdl);
    return;
  }
  esp_netif_set_hostname(eth_netif, "bms-tester");
  esp_netif_dhcpc_start(eth_netif);  // no-op if already running; ignored

  esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, ws_eth_handler,
                             nullptr);
  esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, ws_eth_handler,
                             nullptr);

  // 5. Go. Link events and DHCP do the rest asynchronously; failure here
  //    just means no Ethernet (link/IP events never arrive).
  esp_eth_start(eth_hdl);
}

bool ws_eth_up() { return s_eth_up; }

const char *ws_eth_ip_str() { return s_eth_up ? s_eth_ip : ""; }

#endif  // ARDUINO
