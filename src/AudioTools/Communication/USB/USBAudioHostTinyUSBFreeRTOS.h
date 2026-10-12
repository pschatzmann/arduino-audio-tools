#pragma once
#include "AudioTools/Communication/USB/USBAudioHostTinyUSB.h"
#include "AudioTools/Concurrency/RTOS/BufferRTOS.h"
#include "AudioTools/Concurrency/RTOS/Task.h"

namespace audio_tools {

/**
 * @brief Variant of USBAudioHostTinyUSB for FreeRTOS (e.g. RP2040 +
 * FreeRTOS, or ESP32 with the Adafruit TinyUSB host stack).
 *
 * Replaces the SPSC ring buffers with FreeRTOS StreamBuffers and launches a
 * dedicated FreeRTOS task which runs the USB host stack once per 1 ms tick,
 * so that the isochronous transfers keep running independently of the
 * audio processing.
 *
 * @note I recommend to use the USBAudioHostStream type alias instead of
 * this class directly.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostTinyUSBFreeRTOS : public USBAudioHostTinyUSB {
 public:
  USBAudioHostTinyUSBFreeRTOS() = default;
  USBAudioHostTinyUSBFreeRTOS(USBAudioConfig cfg) : USBAudioHostTinyUSB(cfg) {}
  USBAudioHostTinyUSBFreeRTOS(Adafruit_USBH_Host& host)
      : USBAudioHostTinyUSB(host) {}
  ~USBAudioHostTinyUSBFreeRTOS() { stopUsbTask(); }

  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_rtos_; }
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_rtos_; }

 protected:
  // writeMaxWait=0 initially; startUsbTask() upgrades to blocking once the
  // dedicated task is actively draining the buffer.
  BufferRTOS<uint8_t> buffer_tx_rtos_{0, 0, 0, 0};
  BufferRTOS<uint8_t> buffer_rx_rtos_{0, 1, 0, 0};
  Task usb_task_;

  bool beginUSB() override {
    bool ok = USBAudioHostTinyUSB::beginUSB();
    if (ok && config_.enable_usb_task) startUsbTask();
    return ok;
  }

  void resizeBuffers() override {
    size_t size = (size_t)audioPacketSize() * config_.fifo_packets;
    if (isEpOutEnabled()) buffer_tx_rtos_.resize(size);
    if (isEpInEnabled()) buffer_rx_rtos_.resize(size);
  }

  void startUsbTask() {
    if (usb_task_.getTaskHandle() != nullptr) return;
    LOGI("Starting USB host task: stack=%d, priority=%d",
         config_.usb_task_stack_size, config_.usb_task_priority);
    usb_task_.create("tuh", config_.usb_task_stack_size,
                     config_.usb_task_priority);
    usb_task_.begin([this]() {
      processUSB();
      vTaskDelay(1);
    });
    usb_task_active_ = true;
    buffer_tx_rtos_.setWriteMaxWait((TickType_t)config_.usb_task_write_wait_ms);
  }

  void stopUsbTask() {
    if (usb_task_.getTaskHandle() == nullptr) return;
    usb_task_active_ = false;
    usb_task_.remove();
    buffer_tx_rtos_.setWriteMaxWait(0);
  }
};

}  // namespace audio_tools
