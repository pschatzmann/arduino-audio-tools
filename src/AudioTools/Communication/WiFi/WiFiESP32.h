#pragma once

#include "esp_http_client.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace audio_tools {

/**
 * @brief Login to Wifi using the ESP32 IDF functionality. This can be
 * accessed with the global object IDF_WIFI
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */
class WiFiESP32 {
 public:
  bool begin(const char* ssid, const char* password) {
    TRACEI();
    if (is_open) return true;
    if (!setupWIFI(ssid, password)) {
      LOGE("setupWIFI failed");
      return false;
    }

    return true;
  }

  void end() {
    TRACED();
    if (is_open) {
      TRACEI();
      esp_wifi_stop();
      esp_wifi_deinit();
    }
    is_open = false;
  }

  void setPowerSave(wifi_ps_type_t powerSave) { power_save = powerSave; }

  bool isConnected() { return is_open; }

 protected:
  volatile bool is_open = false;
  esp_ip4_addr_t ip = {0};
  wifi_ps_type_t power_save = WIFI_PS_NONE;

  bool setupWIFI(const char* ssid, const char* password) {
    assert(ssid != nullptr);
    assert(password != nullptr);
    LOGI("setupWIFI: %s", ssid);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      ret = nvs_flash_init();
    }

    if (esp_netif_init() != ESP_OK) {
      LOGE("esp_netif_init");
      return false;
    };

    if (esp_event_loop_create_default() != ESP_OK) {
      LOGE("esp_event_loop_create_default");
      return false;
    };

    esp_netif_t* itf = esp_netif_create_default_wifi_sta();
    if (itf == nullptr) {
      LOGE("esp_netif_create_default_wifi_sta");
      return false;
    };

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_sta_event_handler, this, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_sta_event_handler, this, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
      LOGE("esp_wifi_init");
      return false;
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(power_save);

    wifi_config_t sta_config;
    memset(&sta_config, 0, sizeof(wifi_config_t));
    strncpy((char*)sta_config.sta.ssid, ssid, 32);
    strncpy((char*)sta_config.sta.password, password, 32);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);

    // start wifi
    bool rc = esp_wifi_start() == ESP_OK;
    if (!rc) {
      LOGE("esp_wifi_start");
    }
    return rc;
  }

  static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiESP32* self = (WiFiESP32*)arg;

    if (event_base == WIFI_EVENT) {
      switch (event_id) {
        case WIFI_EVENT_STA_START:
          LOGI("WIFI_EVENT_STA_START");
          esp_wifi_connect();
          break;
        case WIFI_EVENT_STA_DISCONNECTED:
          LOGI("WIFI_EVENT_STA_DISCONNECTED");
          esp_wifi_connect();
          break;
      }
    } else if (event_base == IP_EVENT) {
      switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
          ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
          self->ip = event->ip_info.ip;
          self->is_open = true;
          LOGI("==> Station connected with IP: " IPSTR ", GW: " IPSTR
               ", Mask: " IPSTR ".",
               IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw),
               IP2STR(&event->ip_info.netmask));
          break;
        }
      }
    }
  }

} static IDF_WIFI;

} // namespace audio_tools
