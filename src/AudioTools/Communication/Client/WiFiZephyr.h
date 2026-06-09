#pragma once
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/util.h>

#include "AudioLogger.h"

// ---------------------------------------------------------------------------
// Kconfig guard — fail at compile time if prerequisites are missing
// ---------------------------------------------------------------------------
BUILD_ASSERT(IS_ENABLED(CONFIG_NETWORKING),
             "WiFiZephyr requires CONFIG_NETWORKING=y");
BUILD_ASSERT(IS_ENABLED(CONFIG_NET_L2_WIFI_MGMT),
             "WiFiZephyr requires CONFIG_NET_L2_WIFI_MGMT=y");
BUILD_ASSERT(IS_ENABLED(CONFIG_WIFI), "WiFiZephyr requires CONFIG_WIFI=y");
BUILD_ASSERT(IS_ENABLED(CONFIG_NET_DHCPV4),
             "WiFiZephyr requires CONFIG_NET_DHCPV4=y");

namespace audio_tools {

/**
 * @brief Login to WiFi using Zephyr's wifi_mgmt / net_mgmt API.
 *        Access via the global object ZEPHYR_WIFI.
 * @note the following settings need to be active:
 *      CONFIG_NETWORKING=y
 *      CONFIG_NET_L2_WIFI_MGMT=y
 *      CONFIG_WIFI=y
 *      CONFIG_NET_DHCPV4=y
 *
 * @author Phil Schatzmann (ported to Zephyr)
 * @ingroup http
 * @copyright GPLv3
 */
class WiFiZephyr {
 public:
  bool begin(const char* ssid, const char* password) {
    TRACEI();
    if (_is_open) return true;
    if (!setupWIFI(ssid, password)) {
      LOGE("setupWIFI failed");
      return false;
    }
    return true;
  }

  void end() {
    TRACED();
    if (_is_open) {
      TRACEI();
      struct net_if* iface = net_if_get_default();
      if (iface) {
        net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, nullptr, 0);
        net_if_down(iface);
      }
    }
    _is_open = false;
  }

  /**
   * Power-save mode — maps to Zephyr's WIFI_PS_* values:
   *   WIFI_PS_DISABLED  (= WIFI_PS_NONE equivalent)
   *   WIFI_PS_ENABLED
   */
  void setPowerSave(enum wifi_ps mode) { _power_save = mode; }

  bool isConnected() { return _is_open; }

  /** Returns the assigned IPv4 address (valid after isConnected() == true). */
  const struct in_addr& ip() const { return _ip; }

 protected:
  volatile bool _is_open = false;
  struct in_addr _ip = {};
  enum wifi_ps _power_save = WIFI_PS_DISABLED;

  // net_mgmt callback handles — kept so we can unregister on end()
  struct net_mgmt_event_callback _wifi_cb;
  struct net_mgmt_event_callback _ip_cb;

  // ----------------------------------------------------------------
  // Setup
  // ----------------------------------------------------------------

  bool setupWIFI(const char* ssid, const char* password) {
    assert(ssid != nullptr);
    assert(password != nullptr);
    LOGI("setupWIFI: %s", ssid);

    struct net_if* iface = net_if_get_default();
    if (!iface) {
      LOGE("No default network interface");
      return false;
    }

    // Register event callbacks
    net_mgmt_init_event_callback(
        &_wifi_cb, wifi_event_handler,
        NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&_wifi_cb);

    net_mgmt_init_event_callback(&_ip_cb, ip_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&_ip_cb);

    // Store 'this' so static callbacks can reach the instance
    _instance = this;

    // Apply power-save before connecting
    struct wifi_ps_params ps_params = {};
    ps_params.enabled = _power_save;
    net_mgmt(NET_REQUEST_WIFI_PS, iface, &ps_params, sizeof(ps_params));

    // Build connection parameters
    struct wifi_connect_req_params params = {};
    params.ssid = (const uint8_t*)ssid;
    params.ssid_length = (uint8_t)strlen(ssid);
    params.psk = (const uint8_t*)password;
    params.psk_length = (uint8_t)strlen(password);
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;  // WPA/WPA2-PSK

    int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (rc != 0) {
      LOGE("NET_REQUEST_WIFI_CONNECT failed: %d", rc);
      return false;
    }

    return true;
  }

  // ----------------------------------------------------------------
  // Static event handlers
  // ----------------------------------------------------------------

  /** Handles WIFI_CONNECT_RESULT and WIFI_DISCONNECT_RESULT. */
  static void wifi_event_handler(struct net_mgmt_event_callback* cb,
                                 uint32_t mgmt_event, struct net_if* iface) {
    WiFiZephyr* self = _instance;
    if (!self) return;

    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
      const struct wifi_status* status = (const struct wifi_status*)cb->info;
      if (status && status->status == 0) {
        LOGI("WiFi connected — waiting for IP...");
        // is_open is set in ip_event_handler once DHCP completes
      } else {
        LOGE("WiFi connection failed (status=%d)",
             status ? status->status : -1);
        // Auto-retry, mirroring ESP32 behavior
        net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, nullptr, 0);
      }
    } else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
      LOGI("WiFi disconnected — reconnecting...");
      self->_is_open = false;
      // Auto-reconnect (mirrors WIFI_EVENT_STA_DISCONNECTED handler)
      net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, nullptr, 0);
    }
  }

  /** Handles NET_EVENT_IPV4_ADDR_ADD (equivalent to IP_EVENT_STA_GOT_IP). */
  static void ip_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface) {
    WiFiZephyr* self = _instance;
    if (!self) return;

    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
      // Walk the interface's IPv4 address list to find the assigned addr
      struct net_if_ipv4* ipv4 = iface->config.ip.ipv4;
      if (!ipv4) return;

      for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
        struct net_if_addr* addr = &ipv4->unicast[i];
        if (addr->is_used && addr->addr_type == NET_ADDR_DHCP &&
            addr->address.family == AF_INET) {
          self->_ip = addr->address.in_addr;
          self->_is_open = true;

          char ip_str[NET_IPV4_ADDR_LEN];
          net_addr_ntop(AF_INET, &self->_ip, ip_str, sizeof(ip_str));
          LOGI("==> Station connected with IP: %s", ip_str);
          break;
        }
      }
    }
  }

  // Single-instance pointer for static callbacks (same pattern as ESP32's
  // `arg`)
  static WiFiZephyr* _instance;
};

// Out-of-line definition of the static member
inline WiFiZephyr* WiFiZephyr::_instance = nullptr;

// Global instance — mirrors `IDF_WIFI`
static WiFiZephyr WIFI;

}  // namespace audio_tools
