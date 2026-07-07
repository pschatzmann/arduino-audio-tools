#pragma once
#include <Adafruit_TinyUSB.h>

#include <cstring>

#include "AudioTools/Communication/USB/USBAudioDeviceBase.h"
#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"

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
 * Uses lock-free SPSC ring buffers — suitable for bare-metal or the
 * arduino-pico core-1 `loop1()` pattern where producer/consumer assignment
 * is fixed.  For FreeRTOS use USBAudioDeviceTinyUSBFreeRTOS instead.
 *
 * ## Board settings (arduino-pico + Adafruit TinyUSB)
 * - Board     : "Raspberry Pi Pico" (earlephilhower/arduino-pico)
 * - USB Stack : **Adafruit TinyUSB**
 * 
 * @note I recommend to use the USBAudioStream type alias instead of this class
 * directly, so that the code is portable to other platforms.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceTinyUSB : public USBAudioDeviceBase,
                              public Adafruit_USBD_Interface {
 public:
  USBAudioDeviceTinyUSB() = default;
  USBAudioDeviceTinyUSB(USBAudioConfig cfg) : USBAudioDeviceBase(cfg) {}

  /**
   * Called by Adafruit's addInterface() to build our UAC2 descriptor block
   * into the shared configuration descriptor buffer.
   *
   * When buf is non-NULL, allocate the interface number from TinyUSBDevice
   * so it is unique across all registered interfaces. Endpoint numbers come
   * from config_.ep_in/ep_out/ep_fb/ep_int, which default to a fixed,
   * per-platform value (USB_AUDIO_EP_* in USBAudioConfig.h) on platforms
   * that need one (e.g. nRF52's hardwired ISO endpoint 8). On platforms
   * without such a constraint the default is -1, meaning "not fixed" --
   * in that case we allocate the endpoint dynamically here instead, so it
   * stays unique across all registered interfaces (e.g. when combined with
   * CDC). Either way the caller can still override these fields explicitly
   * before begin() to pin a specific address.
   * When buf is NULL, return the descriptor length for sizing only.
   */
  uint16_t getInterfaceDescriptor(uint8_t /*itfnum_deprecated*/, uint8_t* buf,
                                  uint16_t bufsize) override {
    if (buf) {
      config_.itf_num_ac = TinyUSBDevice.allocInterface(numInterfaces());
      if (config_.enable_ep_in) {
        if (config_.ep_in < 0)
          config_.ep_in = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
      } else if (this->isFeedbackEpEnabled()) {
        if (config_.ep_fb < 0)
          config_.ep_fb = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
      }
      if (config_.enable_ep_out) {
        if (config_.ep_out < 0)
          config_.ep_out = TinyUSBDevice.allocEndpoint(TUSB_DIR_OUT);
      }
      if (config_.enable_interrupt_ep) {
        if (config_.ep_int < 0)
          config_.ep_int = TinyUSBDevice.allocEndpoint(TUSB_DIR_IN);
      }
    }
    uint16_t len = getDescriptor(buf);
    assert(bufsize >= len);
    return len;
  }

 protected:
  RingBufferSPSC<uint8_t> buffer_tx_;
  RingBufferSPSC<uint8_t> buffer_rx_;

  bool beginUSB() override {
    TinyUSBDevice.setID(config_.vid, config_.pid);
    TinyUSBDevice.setManufacturerDescriptor(config_.manufacturer);
    TinyUSBDevice.setProductDescriptor(config_.product);
    TinyUSBDevice.setSerialDescriptor(config_.serial);
    setupDescrBuffer();
    return TinyUSBDevice.addInterface(*this);
  }

  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_; }
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_; }

  void resizeBuffers() override {
    uint16_t block_sz = packetSize();
    uint8_t  block_cnt = config_.fifo_packets;
    if (isEpInEnabled())  buffer_tx_.resize(block_sz * block_cnt);
    if (isEpOutEnabled()) buffer_rx_.resize(block_sz * block_cnt);
  }

  void setupDescrBuffer() {
    static uint8_t buffer[USB_DESCR_MAX_LEN];
    TinyUSBDevice.setConfigurationBuffer(buffer, USB_DESCR_MAX_LEN);
  }
};

}  // namespace audio_tools
