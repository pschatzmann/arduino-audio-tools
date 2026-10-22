#pragma once
#include "AudioTools/CoreAudio/BaseStream.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "RHGenericDriver.h" // https://www.airspayce.com/mikem/arduino/RadioHead/

namespace audio_tools {

/**
 * @brief Arduino Stream which is using the RadioHead library to send and
 * receive data. We use the driver API directly.
 *
 * In TX_MODE the written data is collected and sent in messages of
 * maxMessageLength() bytes: call flush() to send a partially filled message.
 * In RX_MODE each received message is buffered, so that it can be read in
 * chunks of any size.
 *
 * @note Supported only on Arduino platforms with support for the RadioHead
 * library and the related hardware (e.g. ESP32 with LoRa, nRF24L01, etc.)!
 * @note Requires: 👉 https://www.airspayce.com/mikem/arduino/RadioHead/
 *
 * @ingroup communications
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class RadioHeadStream : public Stream {
 public:
  RadioHeadStream(RHGenericDriver& driver, RxTxMode mode = TX_MODE) {
    setDriver(driver);
    setMode(mode);
  }

  void setDriver(RHGenericDriver& driver) { p_driver = &driver; }

  /// Defines if we send (TX_MODE) or receive (RX_MODE)
  void setMode(RxTxMode mode) { this->mode = mode; }

  bool begin(RxTxMode mode) {
    setMode(mode);
    return begin();
  }

  bool begin() {
    if (p_driver == nullptr) return false;
    if (!p_driver->init()) return false;
    buffer.resize(p_driver->maxMessageLength());
    buffer.reset();
    // the driver switches to rx in available() and to tx in send()
    if (mode == RX_MODE) p_driver->available();
    return true;
  }

  void end() {
    if (p_driver == nullptr) return;
    flush();
    p_driver->sleep();
  }

  int available() override {
    if (mode == TX_MODE || p_driver == nullptr) return 0;
    if (buffer.isEmpty()) receive();
    return buffer.available();
  }

  size_t readBytes(uint8_t* data, size_t len) override {
    if (mode == TX_MODE || p_driver == nullptr) return 0;
    size_t processed = 0;
    while (processed < len) {
      if (buffer.isEmpty() && !receive()) break;
      processed += buffer.readArray(data + processed, len - processed);
    }
    return processed;
  }

  int read() override {
    uint8_t c;
    return readBytes(&c, 1) == 1 ? c : -1;
  }

  int peek() override {
    if (available() == 0) return -1;
    uint8_t c;
    return buffer.peek(c) ? c : -1;
  }

  int availableForWrite() override {
    if (mode == RX_MODE || p_driver == nullptr) return 0;
    return p_driver->maxMessageLength();
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (mode == RX_MODE || p_driver == nullptr || buffer.size() == 0) return 0;
    size_t processed = 0;
    while (processed < len) {
      processed += buffer.writeArray(data + processed, len - processed);
      if (buffer.isFull()) send();
    }
    return processed;
  }

  size_t write(uint8_t c) override { return write(&c, 1); }

  /// Sends the buffered data
  void flush() override {
    if (mode == TX_MODE && !buffer.isEmpty()) send();
  }

 protected:
  RHGenericDriver* p_driver = nullptr;
  RxTxMode mode = TX_MODE;
  SingleBuffer<uint8_t> buffer{0};

  /// Copies the next received message into the buffer
  bool receive() {
    if (!p_driver->available()) return false;
    buffer.reset();
    uint8_t len = (uint8_t)buffer.size();
    if (!p_driver->recv(buffer.data(), &len)) return false;
    buffer.setAvailable(len);
    return len > 0;
  }

  void send() {
    p_driver->send(buffer.data(), buffer.available());
    p_driver->waitPacketSent();
    buffer.reset();
  }
};

}  // namespace audio_tools
