#pragma once
#include "stdint.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

namespace audio_tools {

/**
 * @brief Arduino-compatible IPAddress class implemented in pure C++.
 *
 * Supports IPv4 only. Drop-in replacement for Arduino's IPAddress
 * for use on platforms without the Arduino core (e.g. Zephyr, Linux).
 *
 * Usage:
 *   IPAddress ip(192, 168, 1, 1);
 *   IPAddress ip(0xC0A80101);          // same, from uint32_t
 *   IPAddress ip("192.168.1.1");       // from string
 *   IPAddress ip;                      // 0.0.0.0
 *
 *   ip[0]                              // 192
 *   (uint32_t)ip                       // raw big-endian uint32
 *   ip.toString()                      // "192.168.1.1"
 *   if (ip) { ... }                    // false if 0.0.0.0
 *   ip == other                        // comparison
 */
class IPAddress {
 public:
  // -------------------------------------------------------------------------
  // Constructors
  // -------------------------------------------------------------------------

  /// 0.0.0.0
  IPAddress() { _addr.dword = 0; }

  /// From four octets
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    _addr.bytes[0] = a;
    _addr.bytes[1] = b;
    _addr.bytes[2] = c;
    _addr.bytes[3] = d;
  }

  /// From a raw uint32 in host byte order (same as Arduino)
  explicit IPAddress(uint32_t dword) { _addr.dword = dword; }

  /// From a 4-byte array
  explicit IPAddress(const uint8_t octets[4]) {
    std::memcpy(_addr.bytes, octets, 4);
  }

  /// From a dotted-decimal string; leaves address as 0.0.0.0 on parse failure
  explicit IPAddress(const char* str) {
    _addr.dword = 0;
    fromString(str);
  }

  // -------------------------------------------------------------------------
  // Assignment
  // -------------------------------------------------------------------------

  IPAddress& operator=(uint32_t dword) {
    _addr.dword = dword;
    return *this;
  }

  IPAddress& operator=(const uint8_t octets[4]) {
    std::memcpy(_addr.bytes, octets, 4);
    return *this;
  }

  IPAddress& operator=(const char* str) {
    fromString(str);
    return *this;
  }

  // -------------------------------------------------------------------------
  // Accessors
  // -------------------------------------------------------------------------

  /// Octet access (read/write)
  uint8_t& operator[](int i) { return _addr.bytes[i]; }
  uint8_t  operator[](int i) const { return _addr.bytes[i]; }

  /// Raw uint32 (host byte order, same layout as Arduino)
  operator uint32_t() const { return _addr.dword; }

  /// True if address is non-zero
  explicit operator bool() const { return _addr.dword != 0; }

  // -------------------------------------------------------------------------
  // Comparison
  // -------------------------------------------------------------------------

  bool operator==(const IPAddress& other) const {
    return _addr.dword == other._addr.dword;
  }

  bool operator!=(const IPAddress& other) const {
    return !(*this == other);
  }

  bool operator==(uint32_t dword) const { return _addr.dword == dword; }
  bool operator!=(uint32_t dword) const { return _addr.dword != dword; }

  bool operator==(const uint8_t octets[4]) const {
    return std::memcmp(_addr.bytes, octets, 4) == 0;
  }

  /// Returns dotted-decimal string e.g. "192.168.1.1"
  std::string toString() const {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  _addr.bytes[0], _addr.bytes[1],
                  _addr.bytes[2], _addr.bytes[3]);
    return buf;
  }

  /// Parses a dotted-decimal string into this address.
  /// @return true on success, false on failure (address unchanged on failure).
  bool fromString(const char* str) {
    if (!str) return false;
    unsigned a, b, c, d;
    if (std::sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    _addr.bytes[0] = (uint8_t)a;
    _addr.bytes[1] = (uint8_t)b;
    _addr.bytes[2] = (uint8_t)c;
    _addr.bytes[3] = (uint8_t)d;
    return true;
  }

  /// Raw byte pointer (read-only)
  const uint8_t* raw() const { return _addr.bytes; }
  uint8_t*       raw()       { return _addr.bytes; }

  // -------------------------------------------------------------------------
  // Well-known addresses
  // -------------------------------------------------------------------------

  static IPAddress any()       { return IPAddress(0u); }
  static IPAddress broadcast() { return IPAddress(255, 255, 255, 255); }
  static IPAddress loopback()  { return IPAddress(127, 0, 0, 1); }

 private:
  union {
    uint8_t  bytes[4];
    uint32_t dword;
  } _addr;
};

const IPAddress INADDR_NONE(0,0,0,0);

} // namespace audio_tools
