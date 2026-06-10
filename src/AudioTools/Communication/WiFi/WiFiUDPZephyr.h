#pragma once

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/util.h>

#include <cstring>

namespace audio_tools {

/**
 * @brief WiFiUDP (Zephyr zsock version)
 * This class provides a UDP interface using Zephyr's socket API (zsock).
 * It mimics the Arduino WiFiUDP API for compatibility.
 * @author Phil Schatzmann
 * @ingroup http
 * @copyright GPLv3
 */

class WiFiUDP {
 public:
  WiFiUDP() = default;

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

  void stop() {
    if (sock >= 0) {
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

  int peek() {
    if (rx_offset >= rx_len) return -1;
    return rx_buffer[rx_offset];
  }

  // -------------------------
  // Remote info
  // -------------------------
  char* remoteIP(char* buf) {
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
};

}  // namespace audio_tools