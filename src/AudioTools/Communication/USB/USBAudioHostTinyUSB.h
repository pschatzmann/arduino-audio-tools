#pragma once
#include "AudioTools/Communication/USB/USBAudioHostBackendTinyUSB.h"
#include "AudioTools/Communication/USB/USBAudioHostBase.h"
#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"

namespace audio_tools {

/**
 * @brief USB Audio host for the Adafruit TinyUSB host stack: reads audio
 * from an attached USB microphone and/or writes audio to an attached USB
 * speaker.
 *
 * The USB stack is driven by the application: readBytes() and write()
 * call serviceUSB(), which runs tuh_task(). Call serviceUSB() regularly
 * (e.g. in loop()) if you neither read nor write.
 *
 * ## Board settings
 * - RP2040/RP2350 (arduino-pico): USB Stack "Adafruit TinyUSB Host (native)"
 * - Pico-PIO-USB or MAX3421E: set up the Adafruit_USBH_Host in your sketch
 *   and pass it to the constructor; set config.host_rhport accordingly.
 *
 * @note I recommend to use the USBAudioHostStream type alias instead of this
 * class directly, so that the code is portable to other platforms.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostTinyUSB : public USBAudioHostBase {
 public:
  USBAudioHostTinyUSB() = default;
  USBAudioHostTinyUSB(USBAudioConfig cfg) : USBAudioHostBase(cfg) {}
  /// Use the Adafruit_USBH_Host which was set up by the sketch
  USBAudioHostTinyUSB(Adafruit_USBH_Host& host) : backend_impl_(host) {}

  /// Defines the Adafruit_USBH_Host which was set up by the sketch (call
  /// before begin())
  void setHost(Adafruit_USBH_Host& host) { backend_impl_.setHost(host); }

  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_; }

  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_; }

  /** @brief Process pending USB events. Skipped when a dedicated FreeRTOS
   *  task already runs the USB stack (tuh_task() is not re-entrant). */
  void serviceUSB() override {
    if (!usb_task_active_) processUSB();
  }

 protected:
  USBAudioHostBackendTinyUSB backend_impl_;
  RingBufferSPSC<uint8_t> buffer_tx_;
  RingBufferSPSC<uint8_t> buffer_rx_;

  USBAudioHostBackend& backend() override { return backend_impl_; }

  void resizeBuffers() override {
    size_t size = (size_t)audioPacketSize() * config_.fifo_packets;
    if (isEpOutEnabled()) buffer_tx_.resize(size);
    if (isEpInEnabled()) buffer_rx_.resize(size);
  }
};

}  // namespace audio_tools
