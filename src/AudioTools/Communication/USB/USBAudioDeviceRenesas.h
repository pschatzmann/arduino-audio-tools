#pragma once
#include <Arduino.h>

#ifdef ARDUINO_ARCH_RENESAS

// Not "usb/USB.h" via <> -- this core only puts cores/arduino itself (not
// its usb/ subdirectory) on the include path, unlike ESP32's USB.h which
// lives in a separate library and is reachable as <USB.h>.
#include "usb/USB.h"

#include <cstring>

#include "AudioTools/Communication/USB/USBAudioBackendTinyUSB.h"
#include "AudioTools/Communication/USB/USBAudioDeviceBase.h"
#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"

namespace audio_tools {

/**
 * @brief USBAudioDeviceBase subclass for the Arduino UNO R4 / Nano R4
 * (Renesas RA4M1, `arduino:renesas_uno` core).
 *
 * The core's own `USB.cpp` runs TinyUSB internally (via its RUSB2 Device
 * Controller Driver), so `USBAudioBackendTinyUSB` is reused as-is here --
 * this class only supplies the platform-specific descriptor-registration
 * glue, matching the pattern of `USBAudioDeviceESP32`/`USBAudioDeviceTinyUSB`.
 *
 * ## Requires a core with the `__USBGetCustomInterfaceDescriptor` hook
 * Stock `arduino:renesas_uno` (as of release 1.6.0) builds one fixed
 * composite descriptor (CDC + HID + MSD) with no extension point for a
 * third-party class driver -- this class needs the `USB.h`/`USB.cpp` hook
 * that lets an external interface append itself (a weak
 * `__USBGetCustomInterfaceDescriptor()` callback, checked and invoked from
 * `__SetupUSBDescriptor()` the same way HID's `__USBGetHIDReport()` already
 * is). Without that hook in the core, this file will still compile but the
 * Audio interface will silently never appear in the composite descriptor.
 *
 * ## Requires building with `-DNO_USB`
 * `main.cpp` calls `__USBStart()` (which builds and freezes the composite
 * descriptor) automatically at boot, before the sketch's `setup()` runs --
 * unless the board defines `NO_USB` (UNO R4 WiFi already does; UNO R4
 * Minima/Nano R4 do not). Since the descriptor is built lazily but then
 * cached forever (`usbd_desc_cfg` is only ever assigned once), our Audio
 * interface must be registered *before* the first build, which means
 * `beginUSB()` here has to be the one calling `__USBStart()` -- after the
 * sketch's `setup()` has configured the audio format via `begin()`. On
 * boards that auto-start USB before `setup()` runs (Minima/Nano R4), add
 * `-DNO_USB` yourself, e.g. via `arduino-cli compile --build-property
 * "build.extra_flags=-DNO_USB"` or a `boards.local.txt`/`platform.local.txt`
 * override. With `NO_USB` defined, the `Serial` macro maps to a plain UART
 * instead of native-USB CDC -- use `SerialUSB` explicitly if you also want
 * USB serial alongside audio (see `usb-tx.ino`'s composite-device pattern).
 *
 * @note I recommend to use the USBAudioStream type alias instead of this
 * class directly, so that the code is portable to other platforms.
 *
 * @ingroup io
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioDeviceRenesas : public USBAudioDeviceBase {
  friend class Emulated_TinyUSB;

 public:
  USBAudioDeviceRenesas() = default;
  USBAudioDeviceRenesas(USBAudioConfig cfg) : USBAudioDeviceBase(cfg) {}

  /**
   * Called by the core's `__SetupUSBDescriptor()` (via the weak
   * `__USBGetCustomInterfaceDescriptor` hook defined below) to fetch our
   * UAC2 descriptor block. `itfnum` is the first free (0-based) interface
   * number -- CDC/HID/MSD have already claimed lower numbers by the time
   * this runs, matching the same running-interface-index handoff used by
   * `USBAudioDeviceESP32::descriptorCallback`.
   */
  static uint8_t* customDescriptorCallback(uint8_t itfnum, size_t* len,
                                           uint8_t* num_interfaces) {
    auto& dev = getActualInstance();
    dev.config_.itf_num_ac = itfnum;
    static uint8_t desc[USB_DESCR_MAX_LEN];
    uint16_t l = dev.getDescriptor(desc);
    if (len) *len = l;
    if (num_interfaces) *num_interfaces = dev.numInterfaces();
    return desc;
  }

  static USBAudioDeviceRenesas& getActualInstance() {
    return static_cast<USBAudioDeviceRenesas&>(
        USBAudioDeviceBase::activeInstance());
  }

  /** @brief Returns the TX audio buffer (public to match USBAudioDeviceESP32,
   *  so sketches using the portable USBAudioStream alias can call it on
   *  either platform). */
  BaseBuffer<uint8_t>& bufferTx() override { return buffer_tx_; }

  /** @brief Returns the RX audio buffer (public to match USBAudioDeviceESP32,
   *  so sketches using the portable USBAudioStream alias can call it on
   *  either platform). */
  BaseBuffer<uint8_t>& bufferRx() override { return buffer_rx_; }

  /** @brief No-op: `_usbfs_interrupt_handler()` in the core's USB.cpp already
   *  calls `tud_task()` directly from the USB ISR on every interrupt, so
   *  polling it again here would be redundant (tud_task() is not
   *  re-entrant). */
  void serviceUSB() override {}


 protected:
  USBAudioBackendTinyUSB backend_impl_;
  RingBufferSPSC<uint8_t> buffer_tx_;
  RingBufferSPSC<uint8_t> buffer_rx_;
  bool begin_called_ = false;

  USBAudioBackend& backend() override { return backend_impl_; }

  /**
   * @brief Starts the RA4M1's native USB peripheral via the core's
   * `__USBStart()`. Endpoint addresses are fixed (see USB_AUDIO_EP_* in
   * USBAudioConfig.h) since this core has no dynamic endpoint allocator;
   * VID/PID/manufacturer/product/serial in USBAudioConfig are not
   * applicable here -- the whole composite device (CDC + HID + Audio)
   * shares the one board-level identity the core's USB.cpp compiles in.
   */
  bool beginUSB() override {
    if (begin_called_) return true;
    begin_called_ = true;
    __USBStart();
    return true;
  }

  /** @brief Resize buffers as block pools: block size = one USB frame
   *  at the current sample rate, block count = fifo_packets. */
  void resizeBuffers() override {
    uint16_t block_sz = packetSize();
    uint8_t block_cnt = config_.fifo_packets;
    if (isEpInEnabled()) buffer_tx_.resize(block_sz * block_cnt);
    if (isEpOutEnabled()) buffer_rx_.resize(block_sz * block_cnt);
  }
};

/**
 * @brief Minimal wrapper around the Renesas core's TinyUSB API, matching the
 * `TinyUSBDevice` surface used by the portable example sketches on
 * RP2040/ESP32 (isInitialized/begin/mounted/detach/attach).
 */
class Emulated_TinyUSB {
 public:
  bool isInitialized() { return tud_inited(); }
  bool begin(int) { return true; }
  bool mounted() {
    if (!device().begin_called_) return true;
    return tud_mounted();
  }
  bool detach() {
    if (!device().begin_called_) return true;
    return tud_disconnect();
  }
  bool attach() {
    if (!device().begin_called_) {
      device().beginUSB();
      return true;
    }
    return tud_connect();
  }
  USBAudioDeviceRenesas& device() {
    return static_cast<USBAudioDeviceRenesas&>(
        USBAudioDeviceBase::activeInstance());
  }
};

static Emulated_TinyUSB TinyUSBDevice;

/**
 * @brief USBAudioStream type alias for the Arduino UNO R4 / Nano R4.
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
using USBAudioStream = USBAudioDeviceRenesas;

}  // namespace audio_tools

// ── Composite descriptor registration ───────────────────────────────────────
// Overrides the weak __USBGetCustomInterfaceDescriptor hook declared in the
// core's USB.h. Deliberately plain external linkage (not `inline`), for the
// same reason usbd_app_driver_get_cb() in USBAudioBackendTinyUSB.h is not
// `inline`: this header is only ever included from one translation unit in
// practice, and a `static`/`inline` definition here would leave the core's
// own weak default (which returns nullptr, i.e. "no custom interface") free
// to win the link instead in some configurations.
uint8_t* __USBGetCustomInterfaceDescriptor(uint8_t itfnum, size_t* len,
                                           uint8_t* num_interfaces) {
  return audio_tools::USBAudioDeviceRenesas::customDescriptorCallback(
      itfnum, len, num_interfaces);
}

#endif  // ARDUINO_ARCH_RENESAS
