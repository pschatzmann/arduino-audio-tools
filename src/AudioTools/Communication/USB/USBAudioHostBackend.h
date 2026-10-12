#pragma once
#include <cstddef>
#include <cstdint>

#include "AudioTools/Communication/USB/USBAudioBackend.h"
#include "AudioTools/Communication/USB/USBAudioConfig.h"

namespace audio_tools {

/// Role of an isochronous endpoint which is streamed by a host backend.
enum class UsbHostStreamRole : uint8_t {
  /// Audio data from the attached device (microphone): ISO IN
  RxData = 0,
  /// Audio data to the attached device (speaker): ISO OUT
  TxData = 1,
  /// Explicit feedback from the attached speaker: ISO IN
  Feedback = 2
};

/**
 * @brief Upcalls from a USB host backend into the stack independent UAC
 * host class logic (USBAudioHostBase). All methods are called from the USB
 * stack context (TinyUSB: inside tuh_task(); ESP-IDF: the USB host task).
 * @ingroup usb
 */
class USBAudioHostEvents {
 public:
  virtual ~USBAudioHostEvents() = default;

  /// A device was enumerated: `desc` points to the descriptors starting with
  /// an Audio Control interface descriptor and `len` is the number of bytes
  /// which are available after it. Returns the number of bytes which belong
  /// to the audio function (0 if the interface was not accepted).
  virtual uint16_t onAttach(uint8_t daddr, uint16_t vid, uint16_t pid,
                            const uint8_t* desc, uint16_t len) = 0;

  /// The device configuration is active: the class logic can start to
  /// select the alternate settings and open the streaming endpoints.
  virtual void onConfigured(uint8_t daddr) = 0;

  /// The device was removed.
  virtual void onDetach(uint8_t daddr) = 0;

  /// Completion of control() or setInterface(). For IN requests `len` is
  /// the number of bytes which were received into the request buffer.
  virtual void onControlComplete(bool ok, uint16_t len) = 0;

  /// One isochronous IN packet was received (RxData or Feedback).
  virtual void onIsoIn(UsbHostStreamRole role, const uint8_t* data,
                       uint16_t len) = 0;

  /// Provide the next isochronous OUT packet (TxData): fill at most `max`
  /// bytes into `data` and return the packet length.
  virtual uint16_t onIsoOut(uint8_t* data, uint16_t max) = 0;
};

/**
 * @brief Abstract seam for every raw USB host stack call the UAC host class
 * logic (USBAudioHostBase) needs. Concrete subclasses wrap TinyUSB host
 * (USBAudioHostBackendTinyUSB) or the ESP-IDF usb_host library
 * (USBAudioHostBackendESP32).
 *
 * Control requests are asynchronous: the result is reported via
 * USBAudioHostEvents::onControlComplete(), which may also be called before
 * control()/setInterface() returns. Only one control request is in flight
 * at any time. Streaming endpoints are kept running by the backend itself
 * (resubmitting transfers) once openStream() was called.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostBackend {
 public:
  virtual ~USBAudioHostBackend() = default;

  /// Start the host stack and register the event receiver.
  virtual bool begin(USBAudioHostEvents* events, const USBAudioConfig& cfg) = 0;

  /// Stop all streams (the host stack itself keeps running).
  virtual void end() = 0;

  /// Pump the host stack on platforms where the application drives it.
  virtual void task() = 0;

  /// Asynchronous control request on EP0. For IN requests `data` receives
  /// up to setup.wLength bytes; for OUT requests it holds the payload.
  /// `data` must stay valid until onControlComplete().
  virtual bool control(uint8_t daddr, const UsbSetupPacket& setup,
                       uint8_t* data) = 0;

  /// Asynchronous SET_INTERFACE: select alternate setting `alt` of
  /// interface `itf`. Completion is reported via onControlComplete().
  virtual bool setInterface(uint8_t daddr, uint8_t itf, uint8_t alt) = 0;

  /// Open an isochronous endpoint of the currently selected alternate
  /// setting and start streaming it in the indicated role.
  virtual bool openStream(uint8_t daddr, const UsbEndpointDescriptorView& ep,
                          UsbHostStreamRole role) = 0;

  /// Stop streaming the indicated endpoint.
  virtual void closeStream(uint8_t daddr, uint8_t ep_addr) = 0;

  /// Bus speed of the attached device
  virtual UsbSpeed speed() const { return UsbSpeed::Full; }

  /// Milliseconds since start (for control request timeouts).
  virtual uint32_t millis() const = 0;
};

}  // namespace audio_tools
