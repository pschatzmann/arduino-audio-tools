#pragma once

#include "AudioTools/CoreAudio/Buffers.h"
#include "LoRa.h"  // https://github.com/sandeepmistry/arduino-LoRa

// define the default pins used by the transceiver module
#define rst 12
#define dio0 14

namespace audio_tools {

/**
 * @brief LoRa Audio Configuration with default values maximised for
 * speed using the LoRa library from sandeepmistry:
 * https://github.com/sandeepmistry/arduino-LoRa
 * @author Phil Schatzmann
 * @copyright GPLv3

    Heltec LoRa 32 Lora Pins:
        SS: 8
        SCK: 9
        MOSI: 10
        MISO: 11
        RST: 12
        BUSY: 13
        DIO1: 14

 */

struct LoRaConfig : public AudioInfo {
  LoRaConfig() {
    sample_rate = 0;
    channels = 0;
    bits_per_sample = 0;
  }
  int sync_word = 0xF3;  // 0x0-0xFF
  int32_t spi_speed = 8000000;
  int max_size = 200;        // max 256 bytes
  int frequency = 868E6;     // (Asio:433E6, EU:868E6, US: 915E6)
  int tx_power = 20;         // 2-20;
  int spreading_factor = 6;  // 6-12 (6 fastest)
  // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3, 62.5E3, 125E3, 250E3, and
  // 500E3 (fastest)
  int signal_bandwidth = 500E3;
  int pin_ss = SS;
  int pin_rst = rst;
  int pin_dio0 = dio0;
  int max_begin_retry = 10;
};

/**
 * @brief LoRa: Sending and Receiving Audio
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class LoRaStream : public AudioStream {
 public:
  LoRaConfig defaultConfig() {
    LoRaConfig rc;
    return rc;
  }

  void setAudioInfo(AudioInfo info) {
    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    AudioStream::setAudioInfo(info);
  }

  bool begin(LoRaConfig config) {
    cfg = config;
    AudioStream::setAudioInfo(config);
    return begin();
  }

  bool begin() {
    TRACEI();
    is_audio_info_sent = false;
    buffer.resize(cfg.max_size);
    bool rc;
    int count = 0;
    LOGI("LoRa begin...");
    LoRa.setPins(cfg.pin_ss, cfg.pin_rst, cfg.pin_dio0);
    do {
      rc = LoRa.begin(cfg.frequency);
      if (++count > cfg.max_begin_retry) return false;
      if (!rc){
         LOGI("LoRa begin failed");
        delay(800);
      }
    } while(!rc);
    LoRa.setSignalBandwidth(cfg.signal_bandwidth);
    LoRa.setSpreadingFactor(cfg.spreading_factor);
    LoRa.setTxPower(cfg.tx_power);
    LoRa.setSPIFrequency(cfg.spi_speed);
    LoRa.setSyncWord(cfg.sync_word);
    if (rc) LOGI("LoRa begin success");
    return rc;
  }

  void end() {
    TRACEI();
    LoRa.end();
  }

  size_t readBytes(uint8_t* data, size_t len) {
    LOGI("LoRaStream::readBytes: %d", len);
    size_t packetSize = LoRa.parsePacket();
    if (!cfg && packetSize == sizeof(AudioInfo)) {
      readAudioInfo();
      packetSize = LoRa.parsePacket();
    }
    int toRead = min(len, packetSize);
    int read = LoRa.readBytes(data, toRead);
    return read;
  }

  int available() { return cfg.max_size; }

  int availableForWrite() { return cfg.max_size; }

  size_t write(const uint8_t* data, size_t len) {
    LOGI("LoRaStream::write: %d", len);

    if (cfg && !is_audio_info_sent) {
      writeAudioInfo();
      is_audio_info_sent = true;
    }

    for (int j = 0; j < len; j++) {
      buffer.write(data[j]);
      if (buffer.isFull()) {
        LoRa.beginPacket();
        LoRa.write(buffer.data(), buffer.available());
        LoRa.endPacket();
        buffer.clear();
      }
    }
    return len;
  }

 protected:
  LoRaConfig cfg;
  SingleBuffer<uint8_t> buffer;
  bool is_audio_info_sent = false;

  void readAudioInfo() {
    TRACED();
    AudioInfo tmp;
    int read = LoRa.readBytes((uint8_t*)&tmp, sizeof(AudioInfo));
    if (tmp) setAudioInfo(tmp);
  }

  void writeAudioInfo() {
    TRACED();
    LoRa.beginPacket();
    AudioInfo ai = audioInfo();
    LoRa.write((uint8_t*)&ai, sizeof(ai));
    LoRa.endPacket();
  }
};

}  // namespace audio_tools