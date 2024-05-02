#pragma once
#include "AudioTools/BaseStream.h"
#include "RHGenericDriver.h"

namespace audio_tools {


/**
 * @brief Arduino Stream which is using the RadioHead library to send and
 * receive data. We use the river API directly.
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ReadioHeadStream : public Stream {
 public:
  ReadioHeadStream(RHGenericDriver &driver) { setDriver(driver); }

  void setDriver(RHGenericDriver &driver) { p_driver = &driver; }

  bool begin() {
    if (p_driver == nullptr) return false;
    p_driver->setMode(mode == RX_MODE ? RHGenericDriver::RHMode::RHModeRx
                                      : RHGenericDriver::RHMode::RHModeTx);
    return p_driver->init();
  }

  void end() { p_driver->setMode(RHGenericDriver::RHMode::RHModeSleep); }

  int available() override {
    if (mode == TX_MODE) return 0;
    return p_driver->available() ? p_driver->maxMessageLength() : 0;
  }

  size_t readBytes(uint8_t *data, size_t len) override {
    if (mode == TX_MODE) return 0;
    int open = len;
    int processed = 0;
    while (open > 0) {
      uint8_t av = available();
      if (av == 0) break;
      p_driver->recv(data + processed, &av);
      open -= av;
      processed += av;
    }
    return processed;
  }

  int availableForWrite() override {
    if (mode == RX_MODE) return 0;
    return p_driver->maxMessageLength();
  }

  size_t write(const uint8_t *data, size_t len) override {
    if (mode == RX_MODE) return 0;
    int open = len;
    int processed = 0;
    while (open > 0) {
      int av = available();
      if (av == 0) break;
      p_driver->send(data + processed, av);
      open -= av;
      processed += av;
    }
    return processed;
  }

 protected:
  RHGenericDriver *p_driver = nullptr;
  RxTxMode mode;
};

}  // namespace audio_tools