#pragma once
#include <cstring>

#include "AudioTools/Sandbox/USB/USBAudioDeviceBase.h"
#include <Adafruit_TinyUSB.h>

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
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceTinyUSB : public USBAudioDeviceBase,
                               public Adafruit_USBD_Interface {
 public:
  USBAudioDeviceTinyUSB() = default;
  USBAudioDeviceTinyUSB(USBAudioConfig cfg) : USBAudioDeviceBase(cfg) {}

  // ── Adafruit_USBD_Interface ─────────────────────────────────────────────────

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
    }
    uint16_t len = 0;
    const uint8_t* desc = getDescriptor(&len);
    if (!desc) return 0;
    if (buf) {
      if (len > bufsize) return 0;
      memcpy(buf, desc, len);
    }
    return len;
  }

  // ── Platform overrides ──────────────────────────────────────────────────────

  bool beginUSB() override {
    if (!usb_begun_) {
      TinyUSBDevice.setID(config_.vid, config_.pid);
      TinyUSBDevice.setManufacturerDescriptor(config_.manufacturer);
      TinyUSBDevice.setProductDescriptor(config_.product);
      TinyUSBDevice.setSerialDescriptor(config_.serial);
      // bDeviceClass = MISC / IAD is set automatically inside TinyUSBDevice.begin()
      TinyUSBDevice.addInterface(*this);  // calls getInterfaceDescriptor immediately
      TinyUSBDevice.begin();
      usb_begun_ = true;
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

 protected:
  bool usb_begun_ = false;
};

using USBAudioDevice = USBAudioDeviceTinyUSB;

}  // namespace audio_tools
