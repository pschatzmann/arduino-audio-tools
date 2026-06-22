#pragma once
#include <Adafruit_TinyUSB.h>

#include <cstring>

#include "AudioTools/Communication/USB/USBAudioDeviceBase.h"

namespace audio_tools {


/**
 * @brief USBAudioDeviceBase subclass for RP2040 / Adafruit TinyUSB.
 *
 * Adafruit TinyUSB provides non-weak definitions of `tud_descriptor_device_cb`,
 * `tud_descriptor_configuration_cb`, and `tud_descriptor_string_cb` in
 * Adafruit_USBD_Device.cpp.  We must NOT redefine them.  Instead this class
 * inherits from `Adafruit_USBD_Interface` and registers itself via
 * `TinyUSBDevice.addInterface(*this)`.  Adafruit's callback then calls
 * `getInterfaceDescriptor()` to include our UAC2 audio function block inside
 * the overall configuration descriptor.
 *
 * Device identity (VID/PID, strings) is set through `TinyUSBDevice.*` before
 * `TinyUSBDevice.begin()` is called in `beginUSB()`.
 *
 * ## Board settings (arduino-pico + Adafruit TinyUSB)
 * - Board     : "Raspberry Pi Pico" (earlephilhower/arduino-pico)
 * - USB Stack : **Adafruit TinyUSB**
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceTinyUSB : public USBAudioDeviceBase,
                              public Adafruit_USBD_Interface {
 public:
  /// Default construcotr using default config
  USBAudioDeviceTinyUSB() {};

  /// Constructor with config
  USBAudioDeviceTinyUSB(USBAudioConfig cfg) : USBAudioDeviceBase(cfg) {}

  // ── Adafruit_USBD_Interface
  // ─────────────────────────────────────────────────

  /**
   * Called by Adafruit's addInterface() to build our UAC2 descriptor block
   * into the shared configuration descriptor buffer.
   *
   * When buf is non-NULL, allocate interface and endpoint numbers from
   * TinyUSBDevice so they are unique across all registered interfaces.
   * When buf is NULL, return the descriptor length for sizing only.
   * The itfnum_deprecated parameter is ignored — Adafruit now manages
   * interface numbering internally via allocInterface().
   */
  uint16_t getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/, uint8_t* buf,
                                  uint16_t bufsize) override {
    if (buf) {
      config_.itf_num_ac = TinyUSBDevice.allocInterface(numInterfaces());
      if (config_.enable_ep_in)
        config_.ep_in = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
      if (config_.enable_ep_out)
        config_.ep_out = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);
      if (this->isFeedbackEpEnabled())
        config_.ep_fb = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
    }
    uint16_t len = getDescriptor(buf);
    assert(bufsize >= len);
    return len;
  }

 protected:
  bool beginUSB() override {
    TinyUSBDevice.setID(config_.vid, config_.pid);
    TinyUSBDevice.setManufacturerDescriptor(config_.manufacturer);
    TinyUSBDevice.setProductDescriptor(config_.product);
    TinyUSBDevice.setSerialDescriptor(config_.serial);
    setupDescrBuffer();
    if (!TinyUSBDevice.addInterface(*this)) {
      return false;
    }
    return true;
  }

  /** @brief Returns the TX audio buffer (NBuffer block pool). */
  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_; }

  /** @brief Returns the RX audio buffer (NBuffer block pool). */
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_; }

  /** @brief Resize buffers as block pools: block size = one USB frame
   *  at the current sample rate, block count = fifo_packets. */
  void resizeBuffers() override {
    uint16_t block_sz = packetSize();
    uint8_t  block_cnt = config_.fifo_packets;
    if (isEpInEnabled())  buffer_tx_.resize(block_sz * block_cnt);
    if (isEpOutEnabled()) buffer_rx_.resize(block_sz *block_cnt);
  }

  RingBuffer<uint8_t> buffer_tx_{0};
  RingBuffer<uint8_t> buffer_rx_{0};

  void setupDescrBuffer() {
    static uint8_t buffer[USB_DESCR_MAX_LEN];
    TinyUSBDevice.setConfigurationBuffer(buffer, USB_DESCR_MAX_LEN);
  }
};

/**
 * @brief USBAudioStream type alias for Adafruit TinyUSB based systems.  
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
using USBAudioStream = USBAudioDeviceTinyUSB;


}  // namespace audio_tools
