#pragma once

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/util.h>

#include <cstring>
#include "IPAddress.h"

namespace audio_tools {

/**
 * @brief WiFiUDP (Zephyr zsock version)
 * This class provides a UDP interface using Zephyr's socket API (zsock).
 * It mimics the Arduino WiFiUDP API for compatibility.
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */

class WiFiUDPZephyr {
 public:
  WiFiUDPZephyr() = default;
  ~WiFiUDPZephyr() { stop(); }

  // -------------------------
  // Arduino API
  // -------------------------
  uint8_t begin(uint16_t port) {
    local_port = port;

    sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return 0;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (zsock_bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
      zsock_close(sock);
      sock = -1;
      return 0;
    }

    return 1;
  }

  /**
   * @brief Join a multicast group and listen on the given port.
   * @param multicast  Multicast group address (e.g. IPAddress(239,0,0,1))
   * @param port       UDP port to bind to
   * @return 1 on success, 0 on failure
   */
  uint8_t beginMulticast(IPAddress multicast, uint16_t port) {
    local_port = port;

    sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return 0;

    // Allow multiple sockets to share the same port
    int reuse = 1;
    zsock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Bind to INADDR_ANY on the multicast port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (zsock_bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
      zsock_close(sock);
      sock = -1;
      return 0;
    }

    // Join the multicast group on the default interface
    struct ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = htonl((uint32_t)multicast);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (zsock_setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                         sizeof(mreq)) < 0) {
      zsock_close(sock);
      sock = -1;
      return 0;
    }

    multicast_addr = multicast;
    is_multicast = true;
    return 1;
  }

  void stop() {
    if (sock >= 0) {
      // Leave multicast group before closing
      if (is_multicast) {
        struct ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = htonl((uint32_t)multicast_addr);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        zsock_setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                         &mreq, sizeof(mreq));
        is_multicast = false;
      }

      zsock_close(sock);
      sock = -1;
    }
  }

  // -------------------------
  // TX (Arduino style)
  // -------------------------
  int beginPacket(const char* host, uint16_t port) {
    memset(&tx_addr, 0, sizeof(tx_addr));

    tx_addr.sin_family = AF_INET;
    tx_addr.sin_port = htons(port);

    if (zsock_inet_pton(AF_INET, host, &tx_addr.sin_addr) != 1) {
      return 0;
    }

    tx_len = 0;
    return 1;
  }

  /// Begin a packet to an IPAddress (avoids implicit conversion issues)
  int beginPacket(IPAddress ip, uint16_t port) {
    memset(&tx_addr, 0, sizeof(tx_addr));
    tx_addr.sin_family = AF_INET;
    tx_addr.sin_port = htons(port);
    tx_addr.sin_addr.s_addr = htonl((uint32_t)ip);
    tx_len = 0;
    return 1;
  }

  int endPacket() {
    if (sock < 0) return 0;

    // Zero-copy TX attempt: send accumulated buffer once
    struct iovec iov;
    iov.iov_base = tx_buffer;
    iov.iov_len = tx_len;

    struct msghdr msg{};
    msg.msg_name = &tx_addr;
    msg.msg_namelen = sizeof(tx_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int ret = zsock_sendmsg(sock, &msg, 0);

    tx_len = 0;
    return (ret >= 0);
  }

  // buffered write (Arduino compatibility)
  size_t write(uint8_t b) {
    if (tx_len < sizeof(tx_buffer)) {
      tx_buffer[tx_len++] = b;
      return 1;
    }
    return 0;
  }

  size_t write(const uint8_t* data, size_t len) {
    size_t space = sizeof(tx_buffer) - tx_len;
    if (len > space) len = space;

    memcpy(tx_buffer + tx_len, data, len);
    tx_len += len;
    return len;
  }

  // -------------------------
  // RX (ZERO-COPY CORE)
  // -------------------------

  int parsePacket() {
    rx_len = 0;

    sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    // IMPORTANT: MSG_DONTWAIT = non-blocking like Arduino
    int len = zsock_recvfrom(sock, rx_buffer, sizeof(rx_buffer), MSG_DONTWAIT,
                             (sockaddr*)&from, &from_len);

    if (len <= 0) return 0;

    rx_len = len;
    rx_offset = 0;
    last_from = from;

    return len;
  }

  // -------------------------
  // ZERO-COPY RX ACCESS
  // -------------------------

  /// Direct pointer to internal RX buffer (NO COPY)
  const uint8_t* getBuffer() const { return rx_buffer; }

  int available() { return rx_len - rx_offset; }

  int read() {
    if (rx_offset >= rx_len) return -1;
    return rx_buffer[rx_offset++];
  }

  int read(uint8_t* dst, size_t len) {
    int avail = available();
    if (avail <= 0) return 0;

    if (len > (size_t)avail) len = avail;

    memcpy(dst, rx_buffer + rx_offset, len);
    rx_offset += len;
    return len;
  }

  inline size_t readBytes(uint8_t* dest, size_t len){
    return read(dest, len);
  }

  int peek() {
    if (rx_offset >= rx_len) return -1;
    return rx_buffer[rx_offset];
  }

  // -------------------------
  // Remote info
  // -------------------------

  /// Returns the remote IP as an IPAddress (Arduino-compatible)
  IPAddress remoteIP() const {
    return IPAddress(ntohl(last_from.sin_addr.s_addr));
  }

  /// Fills buf with the dotted-decimal remote IP string; returns buf
  char* remoteIP(char* buf = nullptr) {
    if (buf ==nullptr){
        static char buffer[40];
        buf = buffer;
    }
    zsock_inet_ntop(AF_INET, &last_from.sin_addr, buf, INET_ADDRSTRLEN);
    return buf;
  }

  uint16_t remotePort() { return ntohs(last_from.sin_port); }

  void flush() {
    // UDP no-op
  }

  int availableForWrite() { return sizeof(tx_buffer) - tx_len; }

 private:
  int sock = -1;
  uint16_t local_port = 0;

  // TX
  uint8_t tx_buffer[1472];  // MTU-aligned (good for WiFi + ESP32)
  size_t tx_len = 0;
  sockaddr_in tx_addr{};

  // RX (ZERO-COPY BUFFER)
  uint8_t rx_buffer[1472];
  size_t rx_len = 0;
  size_t rx_offset = 0;

  sockaddr_in last_from{};
  bool is_multicast = false;
  IPAddress multicast_addr;

};

using WiFiUDP = WiFiUDPZephyr;
using UDP = WiFiUDPZephyr;

}  // namespace audio_tools
