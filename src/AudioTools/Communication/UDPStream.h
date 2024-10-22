#pragma once
#include <WiFiUdp.h>
#include <esp_now.h>

#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief A UDP class which makes sure that we can use UDP as
 * AudioSource and AudioSink. By default the WiFiUDP object is used and we login
 * to wifi if the ssid and password is provided and we are not already
 * connected.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class UDPStream : public BaseStream {
public:
  /// Default Constructor 
  UDPStream() = default;

  /// @brief  Convinience constructor which defines the optional ssid and
  /// password
  /// @param ssid
  /// @param password
  UDPStream(const char *ssid, const char *password) {
    setSSID(ssid);
    setPassword(password);
  }

  /// @brief Constructor which defines an alternative UDP object. By default we
  /// use WiFiUDP
  /// @param udp
  UDPStream(UDP &udp) { setUDP(udp); }

  /// @brief Defines an alternative UDP object. By default we use WiFiUDP
  /// @param udp
  void setUDP(UDP &udp) { p_udp = &udp; };

  /// Always return 1492 (MTU 1500 - 8 byte header) as UDP packet available to
  /// write
  int availableForWrite() { return 1492; }

  /**
   * Provides the available size of the current package and if this is used up
   * of the next package
   */
  int available() override {
    int size = p_udp->available();
    // if the curren package is used up we prvide the info for the next
    if (size == 0) {
      size = p_udp->parsePacket();
    }
    return size;
  }

  /// Starts to send data to the indicated address / port
  bool begin(IPAddress a, uint16_t port) {
    connect();
    remote_address_ext = a;
    remote_port_ext = port;
    return p_udp->begin(port);
  }

  /// Starts to receive data from/with the indicated port
  bool begin(uint16_t port, uint16_t port_ext = 0) {
    connect();
    remote_address_ext = IPAddress((uint32_t)0);
    remote_port_ext = port_ext != 0 ? port_ext : port;
    return p_udp->begin(port);
  }

   /// Starts to receive data in multicast from/with the indicated address / port
  bool beginMulticast(IPAddress address, uint16_t port) {
    connect();
    return  p_udp->beginMulticast(address,port);   
  }

  /// We use the same remote port as defined in begin for write
  uint16_t remotePort() {
    uint16_t result = p_udp->remotePort();
    return result != 0 ? result : remote_port_ext;
  }

  /// We use the same remote ip as defined in begin for write
  IPAddress remoteIP() {
    // Determine address if it has not been specified
    if ((uint32_t)remote_address_ext == 0) {
      remote_address_ext = p_udp->remoteIP();
    }
    // IPAddress result = p_udp->remoteIP();
    // LOGI("ip: %u", result);
    return remote_address_ext;
  }

  /// Replys will be sent to the initial remote caller
  size_t write(const uint8_t *data, size_t len) override {
    TRACED();
    p_udp->beginPacket(remoteIP(), remotePort());
    size_t result = p_udp->write(data, len);
    p_udp->endPacket();
    return result;
  }

  /// Reads bytes using WiFi::readBytes
  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    size_t avail = available();
    size_t bytes_read = 0;
    if (avail > 0) {
      // get the data now
      bytes_read = p_udp->readBytes((uint8_t *)data, len);
    }
    return bytes_read;
  }

  void setSSID(const char *ssid) { this->ssid = ssid; }

  void setPassword(const char *pwd) { this->password = pwd; }

protected:
  WiFiUDP default_udp;
  UDP *p_udp = &default_udp;
  uint16_t remote_port_ext;
  IPAddress remote_address_ext;
  const char *ssid = nullptr;
  const char *password = nullptr;

  /// connect to WIFI if necessary
  void connect() {
    if (WiFi.status() != WL_CONNECTED && ssid != nullptr &&
        password != nullptr) {
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      // Performance Hack
      // client.setNoDelay(true);
      esp_wifi_set_ps(WIFI_PS_NONE);
    }
  }
};

} // namespace audio_tools
