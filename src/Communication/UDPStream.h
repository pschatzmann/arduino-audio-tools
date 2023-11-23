#pragma once
#include <WiFiUdp.h>
#include <esp_now.h>

#include "AudioTools/AudioStreams.h"
#include "AudioTools/Buffers.h"
#include "AudioBasic/Str.h"


namespace audio_tools {

/**
 * A Simple exension of the WiFiUDP class which makes sure that the basic Stream
 * functioinaltiy which is used as AudioSource and AudioSink
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class UDPStream : public WiFiUDP {
 public:
  UDPStream() = default;

  UDPStream(const char *ssid, const char* password){
    this->ssid = ssid;
    this->password = password;
  }

  /**
   * Always return 1492 (MTU 1500 - 8 byte header) as UDP packet available to write
   */
  int availableForWrite() {
    return 1492;
  }

  /**
   * Provides the available size of the current package and if this is used up
   * of the next package
   */
  int available() override {
    int size = WiFiUDP::available();
    // if the curren package is used up we prvide the info for the next
    if (size == 0) {
      size = parsePacket();
    }
    return size;
  }

  /// Starts to send data to the indicated address / port
  uint8_t begin(IPAddress a, uint16_t port) {
    connect();
    remote_address_ext = a;
    remote_port_ext = port;
    return WiFiUDP::begin(port);
  }

  /// Starts to receive data from/with the indicated port
  uint8_t begin(uint16_t port, uint16_t port_ext = 0) {
    connect();
    remote_address_ext = IPAddress((uint32_t)0u);
    remote_port_ext = port_ext != 0 ? port_ext : port;
    return WiFiUDP::begin(port);
  }

  /// We use the same remote port as defined in begin for write
  uint16_t remotePort() {
    uint16_t result = WiFiUDP::remotePort();
    return result != 0 ? result : remote_port_ext;
  }

  /// We use the same remote ip as defined in begin for write
  IPAddress remoteIP() {
    // Determine address if it has not been specified
    if ((uint32_t)remote_address_ext == 0) {
      remote_address_ext = WiFiUDP::remoteIP();
    }
    // IPAddress result = WiFiUDP::remoteIP();
    // LOGI("ip: %u", result);
    return remote_address_ext;
  }

  /**
   *  Replys will be sent to the initial remote caller
   */
  size_t write(const uint8_t *buffer, size_t size) override {
    TRACED();
    beginPacket(remoteIP(), remotePort());
    size_t result = WiFiUDP::write(buffer, size);
    endPacket();
    return result;
  }
  // Reads bytes using WiFi::readBytes
  size_t readBytes(uint8_t *buffer, size_t length) override {
    TRACED();
    size_t len = available();
    size_t bytes_read = 0;
    if (len>0){
      // get the data now
      bytes_read = WiFiUDP::readBytes((uint8_t*)buffer, length);
    }
  return bytes_read;
}

 protected:
  uint16_t remote_port_ext;
  IPAddress remote_address_ext;
  const char* ssid = nullptr;
  const char* password = nullptr;

  void connect() {
    // connect to WIFI
    if (WiFi.status() != WL_CONNECTED && ssid!=nullptr && password!=nullptr) {
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
      }
    }

    // Performance Hack
    //client.setNoDelay(true);
    esp_wifi_set_ps(WIFI_PS_NONE);

  }
};

}