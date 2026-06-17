#pragma once

#ifdef ESP32

#include <USB.h>

#include <cstring>

#include "AudioTools/Sandbox/USB/USBAudioDeviceBase.h"
#include "esp32-hal-tinyusb.h"

namespace audio_tools {

/**
 * @brief USBAudioDeviceBase subclass for ESP32-S2/S3 using the arduino-esp32 stack.
 *
 * On ESP32 the arduino-esp32 core provides strong definitions of the TinyUSB
 * descriptor callbacks — the user cannot override them.  This class registers
 * the audio function descriptor block via the ESP32-specific API:
 *
 *  - `ESPUSB` (`USB.*`) — configures VID/PID/strings and starts the stack
 *  - `tinyusb_enable_interface(USB_INTERFACE_AUDIO, …)` — injects the UAC2
 *    descriptor into the configuration descriptor the framework builds
 *
 * ## Timing
 * On ESP32, `USB.begin()` is called explicitly by `beginUSB()` which is
 * invoked from `USBAudioStream::begin(cfg, info)`.  The host cannot connect
 * before that, so there is no descriptor-before-config race.
 *
 * ## Board settings
 * - Target : ESP32-S2 or ESP32-S3 (native USB OTG)
 * - `ARDUINO_USB_MODE` must be 0 (OTG / TinyUSB mode)
 *
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceESP32 : public USBAudioDeviceBase {
 public:
  USBAudioDeviceESP32() = default;
  USBAudioDeviceESP32(USBAudioConfig cfg) : USBAudioDeviceBase(cfg) {}

  // ── Platform overrides
  // ──────────────────────────────────────────────────────

  /**
   * @brief Configure and start the ESP32 USB stack.
   *
   * Sets VID/PID/strings on the ESPUSB object, registers the audio class
   * descriptor block with `tinyusb_enable_interface`, and calls `USB.begin()`.
   * Called automatically from `USBAudioStream::begin(cfg, info)`.
   */
  bool beginUSB() override {
    USB.VID(config_.vid);
    USB.PID(config_.pid);
    USB.productName(config_.product);
    USB.manufacturerName(config_.manufacturer);
    USB.serialNumber(config_.serial);
    USB.usbClass(TUSB_CLASS_MISC);
    USB.usbSubClass(MISC_SUBCLASS_COMMON);
    USB.usbProtocol(MISC_PROTOCOL_IAD);
    USB.usbAttributes(
        (uint8_t)(0x80u | (config_.self_powered ? 0x40u : 0x00u)));
    USB.usbPower(config_.max_power_ma);

    uint16_t audio_len = 0;
    getDescriptor(&audio_len);
    tinyusb_enable_interface(USB_INTERFACE_AUDIO, audio_len,
                             descriptorCallback);

    if (!is_active_) {
      if (config_.begin_usb) USB.begin();
      is_active_ = true;
    } else {
      reenumerateUSB();
    }
    return true;
  }

  void reenumerateUSB() override {
    const bool was_mounted = tud_mounted();
    if (was_mounted) tud_disconnect();
    if (was_mounted) {
      delay(20);
      tud_connect();
    }
  }

  // ── ESP32 descriptor callback
  // ─────────────────────────────────────────────── Called by the arduino-esp32
  // framework when it builds the configuration descriptor.  The callback reads
  // the current descriptor at call time, so any format change made via
  // setConfig() before re-enumeration is picked up automatically without
  // re-registering the interface.

  static uint16_t descriptorCallback(uint8_t* dst, uint8_t* itf) {
    auto& dev = static_cast<USBAudioDeviceESP32&>(USBAudioDeviceBase::activeInstance());
    // *itf carries the running interface index: set our base from it so that
    // composite configurations (e.g. CDC + Audio) get non-overlapping numbers.
    if (itf) dev.config_.itf_num_ac = *itf;
    uint16_t len = 0;
    const uint8_t* desc = dev.getDescriptor(&len);
    if (dst && desc) memcpy(dst, desc, len);
    if (itf) *itf += dev.numInterfaces();
    return len;
  }

 protected:
  bool is_active_ = false;
};

using USBAudioDevice = USBAudioDeviceESP32;

}  // namespace audio_tools

#endif  // ESP32
