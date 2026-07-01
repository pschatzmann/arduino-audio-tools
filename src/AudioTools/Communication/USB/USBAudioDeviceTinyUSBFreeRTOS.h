#pragma once
#include "AudioTools/Communication/USB/USBAudioDeviceTinyUSB.h"
#include "AudioTools/Concurrency/RTOS/BufferRTOS.h"
#include "AudioTools/Concurrency/RTOS/Task.h"

namespace audio_tools {

/**
 * @brief Variant of USBAudioDeviceTinyUSB for RP2040 + FreeRTOS.
 *
 * Replaces the SPSC ring buffers with FreeRTOS StreamBuffers (thread-safe
 * under task migration) and launches a dedicated FreeRTOS task that calls
 * `tud_task()` once per 1 ms tick.
 *
 * Two bugs fixed versus calling tud_task() from write():
 * - **Re-entrancy**: tud_task() is not re-entrant; calling it from both
 *   write() and a background task corrupts TinyUSB's event queue.
 *   serviceTinyUSB() is suppressed (via usb_task_active_) while the task runs.
 * - **Starvation**: vTaskDelay(1) — not taskYIELD() — is used so the
 *   max-priority USB task actually sleeps for 1 ms and lets the lower-priority
 *   audio loop run.  One tick matches the USB full-speed frame period exactly.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceTinyUSBFreeRTOS : public USBAudioDeviceTinyUSB {
 public:
  USBAudioDeviceTinyUSBFreeRTOS() = default;
  USBAudioDeviceTinyUSBFreeRTOS(USBAudioConfig cfg) : USBAudioDeviceTinyUSB(cfg) {}
  ~USBAudioDeviceTinyUSBFreeRTOS() { stopUsbTask(); }

 protected:
  // writeMaxWait=0 initially; startUsbTask() upgrades to blocking once the
  // dedicated task is actively draining the buffer.
  BufferRTOS<uint8_t> buffer_tx_rtos_{0, 0, 0, 0};
  BufferRTOS<uint8_t> buffer_rx_rtos_{0, 1, 0, 0};
  Task usb_task_;

  bool beginUSB() override {
    bool ok = USBAudioDeviceTinyUSB::beginUSB();
    if (ok && config_.enable_usb_task) startUsbTask();
    return ok;
  }

  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_rtos_; }
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_rtos_; }

  void resizeBuffers() override {
    uint16_t block_sz = packetSize();
    uint8_t  block_cnt = config_.fifo_packets;
    if (isEpInEnabled())  buffer_tx_rtos_.resize(block_sz * block_cnt);
    if (isEpOutEnabled()) buffer_rx_rtos_.resize(block_sz * block_cnt);
  }

  void startUsbTask() {
    if (usb_task_.getTaskHandle() != nullptr) return;
    usb_task_.create("tud", config_.usb_task_stack_size,
                     config_.usb_task_priority);
    usb_task_.begin([this]() {
      tud_task();
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
