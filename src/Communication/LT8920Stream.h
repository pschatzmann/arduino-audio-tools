#pragma once
#include "AudioTools/AudioStreams.h"
#include "LT8920.h"
#include <SPI.h>

namespace audio_tools {

class LT8920Config {
public:
  RxTxMode mode;
  uint8_t rst_pin = -1;
  uint8_t cs_pin = -1;
  uint8_t pkt_pin = -1;
  // LT8920_1MBPS, LT8920_250KBPS,LT8920_125KBPS,LT8920_62KBPS
  LT8920::DataRate rate = LT8920::DataRate::LT8920_1MBPS;
  uint8_t channel = 0x20; // 0 - 128 (0x80)
  uint16_t default_size = 255 * 4;
  uint8_t power = 0; // 0-0xf
  uint8_t gain = 0;  // 0-0xf
  int send_fail_delay_ms = 10;
};

/**
 * @brief A communications class which uses the LT8920 2.4G RF Transceiver
 * and implements the Arduino Stream API. It depends on the
 * https://github.com/mengguang/LT8920 library.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class LT8920Stream : public AudioStream {
public:

  LT8920Config defaultConfig(RxTxMode mode) {
    LT8920Config cfg;
    cfg.mode = mode;
    return cfg;
  }

  bool begin(LT8920Config cfg) {
    this->config = cfg;
    return begin();
  }

  bool begin() {
    if (config.rst_pin==-1 || config.cs_pin==-1 || config.pkt_pin==-1){
      LOGE("Pins not defined");
      return false;
    }
    if (p_lt == nullptr) {
      p_lt = new LT8920(config.cs_pin, config.pkt_pin, config.rst_pin);
    }
    p_lt->begin();
    p_lt->setCurrentControl(config.power, config.gain);
    p_lt->setDataRate(config.rate);
    p_lt->setChannel(config.channel);
    switch (config.mode) {
    case RX_MODE:
      p_lt->startListening();
      break;
    case TX_MODE:
      break;
    default:
      LOGE("Mode not supported");
      break;
    }
    return true;
  }

  void end() { p_lt->sleep(); }

  int availableForWrite() { return config.default_size; }

  int available() { return p_lt->available() ? config.default_size : 0; }

  size_t write(const uint8_t *data, size_t len) {
    int open = len;
    int processed = 0;
    while (open > 0) {
      int len = min(open, max_size);
      while (!p_lt->sendPacket((uint8_t *)(data + processed), len))
        delay(config.send_fail_delay_ms);
      open -= len;
      processed += len;
    }
    return processed;
  }

  size_t readBytes(uint8_t *data, size_t len) {
    int open = len;
    int processed = 0;
    while (open > 0) {
      if (!available())
        break;
      int len = min(open, max_size);
      int packetSize = p_lt->read(data + processed, len);
      open -= packetSize;
      processed += packetSize;
      p_lt->startListening();
    }
    return processed;
  }

  LT8920 &lt8920() { return *p_lt; }

protected:
  LT8920 *p_lt = nullptr;
  LT8920Config config;
  const int max_size = 255;
};

} // namespace audio_tools