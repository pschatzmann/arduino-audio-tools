#pragma once

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/version.h>

#include "AudioLogger.h"

namespace audio_tools {

class EthernetZephyr {
 public:
  // ------------------------------------------------------------
  // Arduino-style API (SUPPORTED MODES ONLY)
  // ------------------------------------------------------------
  bool begin() {
    struct net_if* iface = net_if_get_default();
    struct net_linkaddr* addr = net_if_get_link_addr(iface);
    uint8_t* mac = addr->addr;
    // uint8_t len = addr->len;
    return begin(mac);
  }

  bool begin(uint8_t* mac) {
    struct in_addr ip = make_ip(0);
    return begin(mac, ip, false);
  }

  bool begin(uint8_t* mac, struct in_addr ip) {
    bool static_ip = (ip.s_addr != 0);
    return begin(mac, ip, static_ip);
  }

  // ------------------------------------------------------------
  // Status API
  // ------------------------------------------------------------

  bool connected() const { return _ip_ready; }

  bool linkUp() const { return _link_up; }

  const struct in_addr& localIP() const { return _ip; }

  const char* localIPString() {
    static char buf[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &_ip, buf, sizeof(buf));
    return buf;
  }

  void end() {
    if (_iface) {
      net_if_down(_iface);
    }
    _ip_ready = false;
    _link_up = false;
  }

  // ------------------------------------------------------------
  // Internal begin
  // ------------------------------------------------------------

 private:
  bool begin(uint8_t* mac, struct in_addr ip, bool static_ip) {
    TRACEI();

    _iface = net_if_get_default();
    if (!_iface) {
      LOGE("No network interface");
      return false;
    }

    // MAC setup
    net_if_set_link_addr(_iface, mac, 6, NET_LINK_ETHERNET);

    // Bring interface up
    net_if_up(_iface);

    _instance = this;

    // Register IP event
    net_mgmt_init_event_callback(&_ip_cb, ip_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&_ip_cb);

    if (static_ip) {
      LOGI("Ethernet: static IP");

      net_if_ipv4_addr_add(_iface, &ip, NET_ADDR_MANUAL, 0);

      _ip = ip;
      _ip_ready = true;
      return true;
    }

    LOGI("Ethernet: DHCP");

    net_dhcpv4_start(_iface);

    return true;
  }

  // ------------------------------------------------------------
  // Event handler
  // ------------------------------------------------------------

  static void ip_event_handler(struct net_mgmt_event_callback* cb,
                               uint64_t event, struct net_if* iface) {
    if (!_instance) return;

    if (event != NET_EVENT_IPV4_ADDR_ADD) return;

    struct net_if_ipv4* ipv4 = iface->config.ip.ipv4;
    if (!ipv4) return;

    for (int i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3, 4, 0)
      struct net_if_addr_ipv4* entry = &ipv4->unicast[i];
      struct net_if_addr* addr = &entry->ipv4;
#else
      struct net_if_addr* addr = &ipv4->unicast[i];
#endif

      if (addr->is_used && addr->addr_type == NET_ADDR_DHCP &&
          addr->address.family == AF_INET) {
        _instance->_ip = addr->address.in_addr;
        _instance->_ip_ready = true;

        char buf[NET_IPV4_ADDR_LEN];
        net_addr_ntop(AF_INET, &_instance->_ip, buf, sizeof(buf));

        LOGI("Ethernet IP: %s", buf);
        break;
      }
    }
  }

 private:
  // ------------------------------------------------------------
  // helpers
  // ------------------------------------------------------------

  static inline struct in_addr make_ip(uint32_t v = 0) {
    struct in_addr a;
    a.s_addr = v;
    return a;
  }

 private:
  struct net_if* _iface = nullptr;

  struct in_addr _ip = {};
  volatile bool _ip_ready = false;
  volatile bool _link_up = false;

  struct net_mgmt_event_callback _ip_cb;

  static EthernetZephyr* _instance;
};

using EthernetClass = EthernetZephyr;

// ------------------------------------------------------------
// static instance
// ------------------------------------------------------------

inline EthernetZephyr* EthernetZephyr::_instance = nullptr;

static EthernetZephyr Ethernet;

}  // namespace audio_tools
