#pragma once
#include "AudioTools/AudioStreams.h"
#include <LoRa.h>
#include <SPI.h>

namespace audio_tools {

class LoRaConfig {
public:
  RxTxMode mode;
  long frequency = 868E6;        // 433E6, 868E6, 915E6
  int tx_power = 20;             // max power
  long signal_bandwidth = 500E3; // Supported values
                                 // are 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3,
                                 // 62.5E3, 125E3, 250E3, and 500E3.
  int spreading_factor = 7; // Supported values are between 6 and 12
  bool async = false;
};

/**
 * @brief A communications class which uses the Semtech LoRa modules and
 * implements the Arduino Stream API. It depends on the
 * https://github.com/sandeepmistry/arduino-LoRa library.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class LoRaStream : public AudioStream {
public:
  LoRaConfig defaultConfig(RxTxMode mode) {
    LoRaConfig cfg;
    cfg.mode = mode;
    return cfg;
  }

  bool begin(LoRaConfig cfg) {
    this->config = cfg;
    bool result = LoRa.begin(cfg.frequency);
    LoRa.setSignalBandwidth(cfg.signal_bandwidth);
    if (config.mode == TX_MODE) {
      LoRa.setTxPower(cfg.tx_power);
    }
    return result;
  }

  void end() { LoRa.end(); }

  int available() { return LoRa.available(); }

  int availableForWrite() { return LoRa.availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) {
    LoRa.beginPacket();
    size_t result = LoRa.write(data, len);
    LoRa.endPacket(config.async);
    return result;
  }

  size_t readBytes(uint8_t *data, size_t len) {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0)
      return 0;
    return LoRa.readBytes(data, len);
  }

  LoRaClass &lora() { return LoRa; }

protected:
  LoRaConfig config;
};

} // namespace audio_tools