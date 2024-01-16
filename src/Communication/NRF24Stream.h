#pragma once
#include "AudioTools/AudioStreams.h"
#include "RF24.h"
#include <SPI.h>

namespace audio_tools {

enum class NRF24Role {
  Unidirectional,
  BidirectionalPrimary,
  BidirectionalSecondary
};

class NRF24Config {
public:
  RxTxMode mode;
  NRF24Role role = Unidirectional;
  rf24_gpio_pin_t ce_pin = -1;
  rf24_gpio_pin_t cs_pin = -1;
  uint8_t default_address[][6] = {"1Node", "2Node"};
  uint8_t default_number = 0;
  int buffer_size = DEFAULT_BUFFER_SIZE;
  _SPI *spi = &SPI;
  bool auto_ack = true;
  rf24_pa_dbm_e pa_level = RF24_PA_MAX;
  rf24_datarate_e data_rate = RF24_2MBPS;
};

/**
 * @brief A communications class which uses the 2.4 GHz NRF24 radio transceiver
 * module and implements the Arduino Stream API. It depends on the
 * https://github.com/nRF24/RF24 library.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class NRF24Stream : public AudioStream {
public:
  NRF24Stream() = default;

  NRF24Config defaultConfig(RxTxMode mode) {
    NRF24Config result;
    result.mode = mode;
    return result;
  }

  bool begin(NRF24Config cfg) {
    TRACED();
    this->cfg = cfg;
    rf_radio.powerUp();
    if (cfg.ce_pin == -1) {
      LOGE("ce_pin not defined");
      return false;
    }
    if (cfg.cs_pin == -1) {
      LOGE("cs_pin not defined");
      return false;
    }
    int rc = rf_radio.begin(cfg.spi, cfg.ce_pin, cfg.cs_pin);
    if (!rc) {
      LOGE("failed to setup NRF24");
      return false;
    }

    switch (cfg.mode) {
    case RX_MODE:
      openReadingPipe();
      startListening();
      break;
    case TX_MODE:
      openWritingPipe();
      stopListening();
      break;
    case RXTX_MODE:
      openReadingPipe();
      openWritingPipe();
      switch (cfg.role) {
      case BidirectionalPrimary:
        stopListening();
        break;
      case BidirectionalSecondary:
        startListening();
        break;
      }
      break;
    default:
      LOGE("Mode not supported");
    }

    // define the power level
    rf_radio.setPALevel(cfg.pa_level);

    // define the transmission speed
    rf_radio.setDataRate(cfg.data_rate);

    // use dynamic payloads
    rf_radio.enableDynamicPayloads();

    // automatic ack
    active = true;
    rf_radio.setAutoAck(cfg.auto_ack);

    return true;
  }

  void end() {
    TRACED();
    rf_radio.powerDown();
    active = false;
  }

  int available() override {
    if (cfg.mode == Unidirectional && mode == TX_MODE)
      return 0;
    return rf_radio.available() ? cfg.buffer_size : 0;
  }

  int availableForWrite() override {
    if (cfg.mode == Unidirectional && mode == RX_MODE)
      return 0;

    return cfg.buffer_size;
  }

  size_t readBytes(uint8_t *buf, size_t len) override {
    LOGD("read: %d", len);
    if (cfg.mode == Unidirectional && mode == TX_MODE)
      return 0;
    if (cfg.mode != Unidirectional)
      startListening();
    int payload = rf_radio.getPayloadSize();
    int open = len;
    int received = 0;
    while (rf_radio.available() && open > len) {
      uint8_t size = rf_radio.getDynamicPayloadSize();
      rf_radio.read((void *)(buf + received), size);
      received += size;
      open - +size;
    }
    return received;
  }

  size_t write(const uint8_t *buf, size_t len) override {
    LOGD("write: %d", len);
    if (cfg.mode == Unidirectional && mode == RX_MODE)
      return 0;
    if (cfg.mode != Unidirectional)
      stopListening();
    // we can send ony max payload chars
    int payload = rf_radio.getPayloadSize();
    int open = len;
    int sent = 0;
    while (open > 0) {
      int send_len = min(payload, open);
      bool ok = rf_radio.write(buf + sent, send_len);
      if (ok) {
        open -= send_len;
        sent += send_len;
      } else {
        LOGW("write failed: open %d", open);
        break;
      }
    }
    return sent;
  }

  operator bool() { return rf_radio.isChipConnected(); }

  RF24 &radio() { return rf_radio; }

  void openReadingPipe() {
    switch (cfg.role) {
    case Unidirectional:
      openReadingPipe(cfg.default_number, cfg.default_address[0]);
    case BidirectionalPrimary:
      openReadingPipe(cfg.default_number, cfg.default_address[0]);
      break;
    case BidirectionalSecondary:
      openReadingPipe(cfg.default_number, cfg.default_address[1]);
      break;
    }
  }

  void openWritingPipe() {
    switch (cfg.role) {
    case Unidirectional:
      openWritingPipe(cfg.default_address[0]);
    case BidirectionalPrimary:
      openWritingPipe(cfg.default_address[1]);
      break;
    case BidirectionalSecondary:
      openWritingPipe(cfg.default_address[0]);
      break;
    }
  }

  void startListening(void) { rf_radio.startListening(); }

  void stopListening(void) { rf_radio.stopListening(); }

protected:
  RF24 rf_radio;
  bool active = false;
  NRF24Config cfg;

  void openReadingPipe(uint8_t number, const uint8_t *address) {
    rf_radio.openReadingPipe(number, address);
  }

  void openWritingPipe(const uint8_t *address) {
    rf_radio.openWritingPipe(address);
  }
};

} // namespace audio_tools