#pragma once

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/util.h>   // for BUILD_ASSERT
#include "AudioTools/CoreAudio/AudioRuntime.h"

// Fail fast at compile time if required Kconfig symbols are missing.
BUILD_ASSERT(IS_ENABLED(CONFIG_NETWORKING),
    "WiFiClientZephyr requires CONFIG_NETWORKING=y");

BUILD_ASSERT(IS_ENABLED(CONFIG_NET_TCP),
    "WiFiClientZephyr requires CONFIG_NET_TCP=y");

BUILD_ASSERT(IS_ENABLED(CONFIG_NET_SOCKETS),
    "WiFiClientZephyr requires CONFIG_NET_SOCKETS=y");

BUILD_ASSERT(IS_ENABLED(CONFIG_DNS_RESOLVER),
    "WiFiClientZephyr requires CONFIG_DNS_RESOLVER=y");


namespace audio_tools {

/**
 * WiFiClient-compatible TCP client for Zephyr RTOS.
 * Header-only implementation — just #include this file.
 *
 * Required prj.conf:
 *   CONFIG_NETWORKING=y
 *   CONFIG_NET_TCP=y
 *   CONFIG_NET_SOCKETS=y
 *   CONFIG_DNS_RESOLVER=y
 */
class WiFiClientZephyr : public Client {
 public:
  /// Default constructor
  WiFiClientZephyr() = default;
  /// Constructor providing a socket (e.g. used by server)
  WiFiClientZephyr(int socket) : _sock(socket) {}

  virtual ~WiFiClientZephyr() { stop(); }

  // ----------------------------------------------------------------
  // Connection management
  // ----------------------------------------------------------------

  /**
   * Open a TCP connection to host:port.
   * @param host  Hostname or dotted-decimal IPv4/IPv6 string.
   * @param port  TCP port (host byte-order).
   * @return 1 on success, 0 on failure (matches Arduino API).
   */
  virtual int connect(const char* host, uint16_t port) {
    stop();

    struct sockaddr_storage addr = {};
    socklen_t addrlen = sizeof(addr);

    if (_resolve(host, port, (struct sockaddr*)&addr, &addrlen) < 0) {
      return 0;
    }

    _sock = zsock_socket(addr.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (_sock < 0) {
      return 0;
    }

    _applyTimeout();

    if (zsock_connect(_sock, (struct sockaddr*)&addr, addrlen) < 0) {
      zsock_close(_sock);
      _sock = -1;
      return 0;
    }

    return 1;
  }

  /**
   * Open a TCP connection to a numeric IPv4 address.
   * @param ip    IPv4 address as a 32-bit big-endian integer.
   * @param port  TCP port (host byte-order).
   * @return 1 on success, 0 on failure.
   */
  virtual int connect(uint32_t ip, uint16_t port) {
    char host[INET_ADDRSTRLEN];
    struct in_addr addr_in;
    addr_in.s_addr = ip;
    zsock_inet_ntop(AF_INET, &addr_in, host, sizeof(host));
    return connect(host, port);
  }

  /** Close the connection and release the socket. */
  void stop() {
    if (_sock >= 0) {
      zsock_close(_sock);
      _sock = -1;
      _has_peek = false;
    }
  }

  /** @return Non-zero if the socket is connected. */
  int connected() {
    if (_sock < 0) return 0;

    // Zero-byte peek detects a graceful remote close without consuming data.
    uint8_t probe;
    int rc = zsock_recv(_sock, &probe, 0, ZSOCK_MSG_PEEK | ZSOCK_MSG_DONTWAIT);
    if (rc == 0) return 0;
    if (rc < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return 0;
    return 1;
  }

  // ----------------------------------------------------------------
  // Write
  // ----------------------------------------------------------------

  /** Send a single byte. @return 1 on success, 0 on failure. */
  size_t write(uint8_t b) { return write(&b, 1); }

  /** Send a buffer. @return Number of bytes actually sent. */
  size_t write(const uint8_t* buf, size_t size) {
    if (_sock < 0 || !buf || size == 0) return 0;

    size_t sent = 0;
    while (sent < size) {
      ssize_t rc = zsock_send(_sock, buf + sent, size - sent, 0);
      if (rc < 0) {
        stop();
        break;
      }
      sent += (size_t)rc;
    }
    return sent;
  }

  size_t print(const char* str) {
    if (!str) return 0;
    return write((const uint8_t*)str, strlen(str));
  }

  size_t println(const char* str) {
    return print(str) + write((const uint8_t*)"\r\n", 2);
  }

  size_t print(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return print(buf);
  }

  size_t println(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return println(buf);
  }

  // ----------------------------------------------------------------
  // Read
  // ----------------------------------------------------------------

  /**
   * @return Bytes available to read without blocking, -1 on error.
   */
  int available() {
    if (_sock < 0) return -1;
    if (_has_peek) return 1;

    // Fast path: FIONREAD
    int bytes = 0;
    if (zsock_ioctl(_sock, ZFD_IOCTL_FIONREAD, &bytes) == 0) {
      return bytes;
    }

    // Fallback: non-blocking peek
    uint8_t b;
    int rc = zsock_recv(_sock, &b, 1, ZSOCK_MSG_DONTWAIT | ZSOCK_MSG_PEEK);
    if (rc > 0) return 1;
    if (rc == 0) {
      stop();
      return 0;
    }
    return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
  }

  /**
   * Read a single byte.
   * @return The byte (0–255), or -1 on error / no data.
   */
  int read() {
    if (_sock < 0) return -1;

    if (_has_peek) {
      _has_peek = false;
      return _peek_byte;
    }

    uint8_t b;
    ssize_t rc = zsock_recv(_sock, &b, 1, 0);
    if (rc == 1) return b;
    if (rc == 0) stop();
    return -1;
  }

  /**
   * Read up to `size` bytes into `buf`.
   * @return Bytes read, 0 on timeout/no-data, -1 on error.
   */
  int read(uint8_t* buf, size_t size) {
    if (_sock < 0 || !buf || size == 0) return -1;

    size_t offset = 0;

    if (_has_peek && size > 0) {
      buf[0] = _peek_byte;
      _has_peek = false;
      offset = 1;
      size -= 1;
    }

    if (size == 0) return (int)offset;

    ssize_t rc = zsock_recv(_sock, buf + offset, size, 0);
    if (rc < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return (int)offset;
      return offset > 0 ? (int)offset : -1;
    }
    if (rc == 0) stop();
    return (int)(offset + (size_t)rc);
  }

  /**
   * Peek at the next byte without consuming it.
   * @return The byte (0–255), or -1 if unavailable.
   */
  int peek() {
    if (_sock < 0) return -1;
    if (_has_peek) return _peek_byte;

    uint8_t b;
    ssize_t rc = zsock_recv(_sock, &b, 1, ZSOCK_MSG_PEEK);
    if (rc == 1) {
      _peek_byte = b;
      _has_peek = true;
      return b;
    }
    if (rc == 0) stop();
    return -1;
  }

  // ----------------------------------------------------------------
  // Socket options
  // ----------------------------------------------------------------

  /**
   * Set the receive timeout (default: 5000 ms).
   * Pass 0 to block indefinitely.
   */
  void setTimeout(uint32_t timeout_ms) {
    _timeout_ms = timeout_ms;
    _applyTimeout();
  }

  /** @return The underlying Zephyr socket fd, or -1 if not open. */
  int fd() const { return _sock; }

 protected:
  int _sock = -1;
  uint32_t _timeout_ms = 5000;
  uint8_t _peek_byte = 0;
  bool _has_peek = false;

  void _applyTimeout() {
    if (_sock < 0) return;
    struct timeval tv = {};
    tv.tv_sec = _timeout_ms / 1000;
    tv.tv_usec = (_timeout_ms % 1000) * 1000;
    zsock_setsockopt(_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  int _resolve(const char* host, uint16_t port, struct sockaddr* out,
               socklen_t* outlen) {
    struct zsock_addrinfo hints = {};
    struct zsock_addrinfo* res = nullptr;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = zsock_getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) return -1;

    memcpy(out, res->ai_addr, res->ai_addrlen);
    *outlen = res->ai_addrlen;
    zsock_freeaddrinfo(res);
    return 0;
  }
};

using WiFiClient = WiFiClientZephyr;

}  // namespace audio_tools
