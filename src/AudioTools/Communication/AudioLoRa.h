#include "AudioTools/CoreAudio/Buffers.h"
#include "LoRa.h"

// define the default pins used by the transceiver module
#define ss 8
#define rst 12
#define dio0 14

namespace audio_tools {

/**
 * @brief LoRa Audio Configuration with default values maximised for
 * speed.
 * @author Phil Schatzmann
 * @copyright GPLv3

    Heltec LoRa 32 Lora Pins:
        NSS: 8
        SCK: 9
        MOSI: 10
        MISO: 11
        RST: 12
        BUSY: 13
        DIO1: 14

 */

struct AudioLoRaConfig : public AudioInfo {
  int32_t spi_speed = 8E6;
  int max_size = 200;
  int frequency = 868E6;  // (433E6, 868E6, 915E6)
  int sync_word = 0xF3;
  int tx_power = 20;          // 2-20;
  int spreading_factor = 12;  // 6-12
  int signal_bandwidth =
      7.8E3;  // 7.8E3, 10.4E3, 15.6E3, 20.8E3, 31.25E3, 41.7E3,
              // 62.5E3, 125E3, 250E3, and 500E3.
  int pin_ss = ss;
  int pin_rst = rst;
  int pin_dio0 = dio0;
  bool process_audio_info = true;
};

/**
 * @brief LoRa Audio Sending and Receiving
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioLoRa : public AudioStream {
 public:
  AudioLoRaConfig defaultConfig() {
    AudioLoRaConfig rc;
    return rc;
  }

  void setAudioInfo(AudioInfo info) {
    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    AudioStream::setAudioInfo(info);
  }

  bool begin(AudioLoRaConfig config) {
    cfg = config;
    AudioStream::setAudioInfo(config);
    return begin();
  }

  bool begin() {
    TRACEI();
    buffer.resize(cfg.max_size);
    LoRa.setSignalBandwidth(cfg.signal_bandwidth);
    LoRa.setSpreadingFactor(cfg.spreading_factor);
    LoRa.setTxPower(cfg.tx_power);
    LoRa.setSPIFrequency(cfg.spi_speed);
    LoRa.setPins(cfg.pin_ss, cfg.pin_rst, cfg.pin_dio0);
    LoRa.setSyncWord(cfg.sync_word);
    bool rc = LoRa.begin(cfg.frequency);
    if (cfg.process_audio_info) {
      writeAudioInfo();
    }
    return rc;
  }

  void end() { LoRa.end(); }

  size_t readBytes(uint8_t* data, size_t len) {
    TRACEI();
    size_t packetSize = LoRa.parsePacket();
    if (cfg.process_audio_info && packetSize == sizeof(AudioInfo)) {
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
    TRACEI();
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
  AudioLoRaConfig cfg;
  SingleBuffer<uint8_t> buffer;

  void readAudioInfo() {
    AudioInfo tmp;
    int read = LoRa.readBytes((uint8_t*)&tmp, sizeof(AudioInfo));
    setAudioInfo(tmp);
  }

  void writeAudioInfo() {
    LoRa.beginPacket();
    AudioInfo ai = audioInfo();
    LoRa.write((uint8_t*)&ai, sizeof(ai));
    LoRa.endPacket();
  }
};

}  // namespace audio_tools