#pragma once

#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "RF24.h"  // https://github.com/nRF24/RF24

namespace audio_tools {

/**
 * @brief NRF24L01 Audio Configuration optimized for low-latency audio
 * streaming. Uses the RF24 library from TMRh20:
 * https://github.com/nRF24/RF24
 *
 * Default settings: 2Mbps, no ACK, no CRC, channel 124 (less crowded
 * upper band), max PA level. With these defaults, mono 16-bit 32kHz
 * audio achieves ~6.5ms latency.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct NRF24Config : public AudioInfo {
  NRF24Config() {
    sample_rate = 0;
    channels = 0;
    bits_per_sample = 0;
  }
  /// CE pin number
  int pin_ce = 9;
  /// CSN pin number
  int pin_csn = 10;
  /// RF channel (0-125). Higher channels (100+) are less crowded
  uint8_t channel = 124;
  /// Data rate: RF24_250KBPS, RF24_1MBPS, RF24_2MBPS
  rf24_datarate_e data_rate = RF24_2MBPS;
  /// PA level: RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
  rf24_pa_dbm_e pa_level = RF24_PA_MAX;
  /// Payload size in bytes (1-32)
  uint8_t payload_size = 32;
  /// Pipe address for communication (5 bytes). TX and RX must match
  uint64_t address = 0xF0F0F0F0E1LL;
  /// RX or TX mode
  RxTxMode mode = TX_MODE;
  /// Enable auto-acknowledgment. Disable for lowest latency
  bool auto_ack = false;
  /// CRC length: RF24_CRC_DISABLED, RF24_CRC_8, RF24_CRC_16
  rf24_crclength_e crc_length = RF24_CRC_DISABLED;
  /// Use writeFast() instead of write() for TX. Lower latency
  bool use_write_fast = true;
  /// Number of retries for begin() initialization
  int max_begin_retry = 10;
};

/**
 * @brief NRF24L01(+) Stream for sending and receiving audio via the
 * nRF24L01 2.4GHz radio module. Optimized for low-latency, high-throughput
 * audio with small 32-byte packets.
 *
 * Works across Teensy, ESP32, Arduino Nano (including variants with
 * integrated nRF24), and other platforms supported by the RF24 library.
 *
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NRF24Stream : public AudioStream {
 public:
  /// Default constructor
  NRF24Stream() = default;

  /// Constructor with an externally created RF24 object
  NRF24Stream(RF24 &radio) : p_radio(&radio) {}

  /// Constructor with an external receive buffer
  NRF24Stream(BaseBuffer<uint8_t> &buffer) : p_rx_buffer(&buffer) {}

  /// Constructor with an externally created RF24 object and receive buffer
  NRF24Stream(RF24 &radio, BaseBuffer<uint8_t> &buffer)
      : p_radio(&radio), p_rx_buffer(&buffer) {}

  /// Sets an external receive buffer
  void setBuffer(BaseBuffer<uint8_t> &buffer) { p_rx_buffer = &buffer; }

  /// Returns the default configuration for the given mode
  NRF24Config defaultConfig(RxTxMode mode = TX_MODE) {
    NRF24Config rc;
    rc.mode = mode;
    return rc;
  }

  /// Updates the audio info and propagates to the base class
  void setAudioInfo(AudioInfo info) override {
    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    cfg.bits_per_sample = info.bits_per_sample;
    AudioStream::setAudioInfo(info);
  }

  /// Starts the stream with the given configuration
  bool begin(NRF24Config config) {
    cfg = config;
    AudioStream::setAudioInfo(config);
    return begin();
  }

  /// Initializes the radio hardware and configures TX or RX mode
  bool begin() override {
    TRACEI();

    if (p_radio == nullptr) {
      p_own_radio = new RF24(cfg.pin_ce, cfg.pin_csn);
      p_radio = p_own_radio;
    }

    bool rc = false;
    for (int count = 0; count < cfg.max_begin_retry; count++) {
      rc = p_radio->begin();
      if (rc) break;
      LOGI("NRF24 begin failed, retrying...");
      delay(500);
    }
    if (!rc) {
      LOGE("NRF24 begin failed after %d retries", cfg.max_begin_retry);
      return false;
    }

    p_radio->setChannel(cfg.channel);
    p_radio->setDataRate(cfg.data_rate);
    p_radio->setPALevel(cfg.pa_level);
    p_radio->setPayloadSize(cfg.payload_size);
    p_radio->setCRCLength(cfg.crc_length);
    p_radio->setAutoAck(cfg.auto_ack);

    if (cfg.mode == TX_MODE) {
      p_radio->openWritingPipe(cfg.address);
      p_radio->stopListening();
    } else {
      p_radio->openReadingPipe(1, cfg.address);
      p_radio->startListening();
    }

    tx_buffer.resize(cfg.payload_size);

    if (cfg.mode == RX_MODE && p_rx_buffer == nullptr) {
      default_rx_buffer.resize(cfg.payload_size * 16);
      p_rx_buffer = &default_rx_buffer;
    }

    LOGI("NRF24 begin success (mode: %s, channel: %d, payload: %d)",
         RxTxModeNames[cfg.mode], cfg.channel, cfg.payload_size);
    return true;
  }

  /// Stops the radio and releases resources
  void end() override {
    TRACEI();
    if (p_radio != nullptr) {
      p_radio->stopListening();
      p_radio->powerDown();
    }
    if (p_own_radio != nullptr) {
      delete p_own_radio;
      p_own_radio = nullptr;
      p_radio = nullptr;
    }
    tx_buffer.resize(0);
  }

  /// Reads received audio data from the RX buffer
  size_t readBytes(uint8_t *data, size_t len) override {
    if (cfg.mode == TX_MODE || p_radio == nullptr) return 0;
    fillBuffer();
    return p_rx_buffer->readArray(data, len);
  }

  /// Returns the number of bytes available to read
  int available() override {
    if (cfg.mode == TX_MODE || p_radio == nullptr) return 0;
    fillBuffer();
    return p_rx_buffer->available();
  }

  /// Returns the number of bytes available to write
  int availableForWrite() override {
    if (cfg.mode == RX_MODE || p_radio == nullptr) return 0;
    return cfg.payload_size;
  }

  /// Writes audio data, buffering into payload-sized packets for transmission
  size_t write(const uint8_t *data, size_t len) override {
    if (cfg.mode == RX_MODE || p_radio == nullptr) return 0;

    for (size_t j = 0; j < len; j++) {
      tx_buffer.write(data[j]);
      if (tx_buffer.isFull()) {
        if (cfg.use_write_fast) {
          p_radio->writeFast(tx_buffer.data(), tx_buffer.available());
        } else {
          p_radio->write(tx_buffer.data(), tx_buffer.available());
        }
        tx_buffer.clear();
      }
    }
    return len;
  }

  /// Provides access to the RF24 radio object for advanced configuration
  RF24 *radio() { return p_radio; }

 protected:
  NRF24Config cfg;
  RF24 *p_radio = nullptr;
  RF24 *p_own_radio = nullptr;
  BaseBuffer<uint8_t> *p_rx_buffer = nullptr;
  RingBuffer<uint8_t> default_rx_buffer{0};
  SingleBuffer<uint8_t> tx_buffer;

  /// Drains all available radio packets into the RX buffer
  void fillBuffer() {
    if (p_rx_buffer == nullptr || p_radio == nullptr) return;
    while (p_radio->available()) {
      uint8_t tmp[32];
      p_radio->read(tmp, cfg.payload_size);
      p_rx_buffer->writeArray(tmp, cfg.payload_size);
    }
  }
};

}  // namespace audio_tools
