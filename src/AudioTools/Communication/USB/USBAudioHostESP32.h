#pragma once
#include <Arduino.h>

#if !(CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
      CONFIG_IDF_TARGET_ESP32P4)
#error "USB Audio Host needs an ESP32 with USB-OTG (S2/S3/P4); for a MAX3421E define USE_TINYUSB to use the Adafruit TinyUSB host stack"
#endif

#if !__has_include("usb/usb_host.h")
#error "USB Audio Host: the ESP-IDF usb_host library is not available"
#endif

#include "AudioTools/Communication/USB/USBAudioHostBackendESP32.h"
#include "AudioTools/Communication/USB/USBAudioHostBase.h"
#include "AudioTools/Concurrency/RTOS/BufferRTOS.h"
#include "AudioTools/Concurrency/RTOS/Task.h"

namespace audio_tools {

/**
 * @brief USB Audio host for ESP32-S2/S3/P4 using the ESP-IDF usb_host
 * library: reads audio from an attached USB microphone and/or writes audio
 * to an attached USB speaker.
 *
 * begin() starts a dedicated FreeRTOS task which processes the USB events
 * and keeps the isochronous transfers running, so serviceUSB() is a no-op.
 *
 * ## Board settings
 * - Target : ESP32-S2, ESP32-S3 or ESP32-P4 (USB-OTG)
 * - ESP32-S3: Tools → USB Mode: "Hardware CDC and JTAG", so that the OTG
 *   port is not used by the TinyUSB device stack
 * - The attached device needs a 5V VBUS supply
 *
 * @note I recommend to use the USBAudioHostStream type alias instead of
 * this class directly, so that the code is portable to other platforms.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostESP32 : public USBAudioHostBase {
 public:
  USBAudioHostESP32() = default;
  USBAudioHostESP32(USBAudioConfig cfg) : USBAudioHostBase(cfg) {}
  ~USBAudioHostESP32() { stopUsbTask(); }

  /** @brief Returns the TX audio buffer (FreeRTOS stream buffer). */
  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_; }

  /** @brief Returns the RX audio buffer (FreeRTOS stream buffer). */
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_; }

  /// Returns the actual core on which the call is running
  int getActualCore() const override { return xPortGetCoreID(); }

  /** @brief No-op: the USB task runs the stack continuously. */
  void serviceUSB() override {}

 protected:
  USBAudioHostBackendESP32 backend_impl_;
  // TX: blocking write (5 ms) so that the copier waits for free space;
  // RX: the USB task never blocks when writing.
  BufferRTOS<uint8_t> buffer_tx_{0, 0, 5, 0};
  BufferRTOS<uint8_t> buffer_rx_{0, 1, 0, 0};
  Task usb_task_;

  USBAudioHostBackend& backend() override { return backend_impl_; }

  bool beginUSB() override {
    if (!USBAudioHostBase::beginUSB()) return false;
    startUsbTask();
    return true;
  }

  void resizeBuffers() override {
    size_t size = (size_t)audioPacketSize() * config_.fifo_packets;
    if (isEpOutEnabled()) buffer_tx_.resize(size);
    if (isEpInEnabled()) buffer_rx_.resize(size);
  }

  void startUsbTask() {
    if (usb_task_.getTaskHandle() != nullptr) return;
    // the ESP32 FreeRTOS uses bytes for the stack size
    int stack = config_.usb_task_stack_size < 4096 ? 4096
                                                   : config_.usb_task_stack_size;
    usb_task_.create("usb_audio_host", stack, config_.usb_task_priority);
    // processUSB() blocks for at most 1 tick waiting for USB events
    usb_task_.begin([this]() { processUSB(); });
    usb_task_active_ = true;
  }

  void stopUsbTask() {
    if (usb_task_.getTaskHandle() == nullptr) return;
    usb_task_active_ = false;
    usb_task_.remove();
  }
};

}  // namespace audio_tools
