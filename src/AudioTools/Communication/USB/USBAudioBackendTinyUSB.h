#pragma once
#include "AudioTools/Communication/USB/USBAudioBackend.h"

extern "C" {
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"
}

namespace audio_tools {

// TinyUSB >= 0.15 (ESP32 IDF v5+, current Adafruit TinyUSB) added an is_isr
// parameter to usbd_edpt_xfer(); older TinyUSB (bundled with some RP2040 /
// SAMD / STM32 cores) does not have it. Detect the new API by the presence
// of the renamed UAC2 IAD-length macro, exactly as USBAudioDeviceBase.h used
// to before this file existed.
#ifdef TUD_AUDIO20_DESC_IAD_LEN
#define TU_AUDIO_EDPT_XFER(rp, ep, buf, sz) usbd_edpt_xfer(rp, ep, buf, sz, false)
#else
#define TU_AUDIO_EDPT_XFER(rp, ep, buf, sz) usbd_edpt_xfer(rp, ep, buf, sz)
#endif

/**
 * @brief USBAudioBackend implementation wrapping the real TinyUSB device
 *        stack. Injected by the ESP32, TinyUSB (RP2040/Adafruit), and
 *        TinyUSBFreeRTOS platform subclasses of USBAudioDeviceBase.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioBackendTinyUSB : public USBAudioBackend {
 public:
  bool openEndpoint(uint8_t rhport, const UsbEndpointDescriptorView& ep) override {
    tusb_desc_endpoint_t d = toTusbDescriptor(ep);
    return usbd_edpt_open(rhport, &d);
  }

  bool closeEndpoint(uint8_t rhport, uint8_t ep_addr) override {
    usbd_edpt_close(rhport, ep_addr);
    return true;
  }

  bool clearStall(uint8_t rhport, uint8_t ep_addr) override {
    usbd_edpt_clear_stall(rhport, ep_addr);
    return true;
  }

  bool usesIsoAlloc() const override {
#ifdef TUP_DCD_EDPT_ISO_ALLOC
    return true;
#else
    return false;
#endif
  }

  bool isoAllocEndpoint(uint8_t rhport, uint8_t ep_addr,
                        uint16_t max_packet_size) override {
    return usbd_edpt_iso_alloc(rhport, ep_addr, max_packet_size);
  }

  bool isoActivateEndpoint(uint8_t rhport,
                           const UsbEndpointDescriptorView& ep) override {
    tusb_desc_endpoint_t d = toTusbDescriptor(ep);
    return usbd_edpt_iso_activate(rhport, &d);
  }

  bool releaseEndpoint(uint8_t rhport, uint8_t ep_addr) override {
    return usbd_edpt_release(rhport, ep_addr);
  }

  bool claimEndpoint(uint8_t rhport, uint8_t ep_addr) override {
    return usbd_edpt_claim(rhport, ep_addr);
  }

  bool transfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer,
               uint16_t length) override {
    return TU_AUDIO_EDPT_XFER(rhport, ep_addr, buffer, length);
  }

  bool controlTransfer(uint8_t rhport, const UsbSetupPacket& request,
                       uint8_t* buffer, uint16_t length) override {
    tusb_control_request_t r = toTusbRequest(request);
    return tud_control_xfer(rhport, &r, buffer, length);
  }

  bool controlStatus(uint8_t rhport, const UsbSetupPacket& request) override {
    tusb_control_request_t r = toTusbRequest(request);
    return tud_control_status(rhport, &r);
  }

  UsbSpeed speed() const override {
    switch (tud_speed_get()) {
      case TUSB_SPEED_FULL:
        return UsbSpeed::Full;
      case TUSB_SPEED_HIGH:
        return UsbSpeed::High;
      case TUSB_SPEED_LOW:
        return UsbSpeed::Low;
      default:
        return UsbSpeed::Unknown;
    }
  }

  bool mounted() const override { return tud_mounted(); }

  void enableSof(uint8_t rhport, bool enable) override {
    usbd_sof_enable(rhport, SOF_CONSUMER_AUDIO, enable);
  }

  // ── The only two places in the whole backend that touch TinyUSB's raw
  //    struct layout directly ────────────────────────────────────────────

  static tusb_desc_endpoint_t toTusbDescriptor(const UsbEndpointDescriptorView& ep) {
    tusb_desc_endpoint_t d{};
    d.bLength = sizeof(tusb_desc_endpoint_t);
    d.bDescriptorType = TUSB_DESC_ENDPOINT;
    d.bEndpointAddress = ep.bEndpointAddress;
    d.bmAttributes.xfer = (uint8_t)ep.xferType;
    d.bmAttributes.sync = ep.sync;
    d.bmAttributes.usage = ep.usage;
    d.wMaxPacketSize = ep.wMaxPacketSize;
    d.bInterval = ep.bInterval;
    return d;
  }

  static UsbSetupPacket fromTusbRequest(tusb_control_request_t const* r) {
    UsbSetupPacket p;
    p.bmRequestType = r->bmRequestType;
    p.bRequest = r->bRequest;
    p.wValue = r->wValue;
    p.wIndex = r->wIndex;
    p.wLength = r->wLength;
    return p;
  }

  static tusb_control_request_t toTusbRequest(const UsbSetupPacket& p) {
    tusb_control_request_t r{};
    r.bmRequestType = p.bmRequestType;
    r.bRequest = p.bRequest;
    r.wValue = p.wValue;
    r.wIndex = p.wIndex;
    r.wLength = p.wLength;
    return r;
  }

  static UsbInterfaceDescriptorView fromTusbInterface(tusb_desc_interface_t const* d) {
    UsbInterfaceDescriptorView v;
    v.bInterfaceNumber = d->bInterfaceNumber;
    v.bAlternateSetting = d->bAlternateSetting;
    v.bNumEndpoints = d->bNumEndpoints;
    v.bInterfaceClass = d->bInterfaceClass;
    v.bInterfaceSubClass = d->bInterfaceSubClass;
    v.bInterfaceProtocol = d->bInterfaceProtocol;
    return v;
  }
};

}  // namespace audio_tools

// ── TinyUSB class-driver registration ───────────────────────────────────────
// This is included after USBAudioDeviceBase.h everywhere it's used, so the
// full class definition (audiod_init/audiod_open/... methods) is available.
#include "AudioTools/Communication/USB/USBAudioDeviceBase.h"

namespace audio_tools {

inline usbd_class_driver_t const* usbAudioGetClassDriver(uint8_t* count) {
  static usbd_class_driver_t driver;
  driver.name = "AUDIO";
  driver.init = []() { USBAudioDeviceBase::activeInstance().audiod_init(); };
  driver.deinit = []() {
    return USBAudioDeviceBase::activeInstance().audiod_deinit();
  };
  driver.reset = [](uint8_t rhport) {
    USBAudioDeviceBase::activeInstance().audiod_reset(rhport);
  };
  driver.open = [](uint8_t rhport, tusb_desc_interface_t const* itf_desc,
                   uint16_t max_len) {
    UsbInterfaceDescriptorView view =
        USBAudioBackendTinyUSB::fromTusbInterface(itf_desc);
    return USBAudioDeviceBase::activeInstance().audiod_open(
        rhport, view, (uint8_t const*)itf_desc, max_len);
  };
  driver.control_xfer_cb = [](uint8_t rhport, uint8_t stage,
                              tusb_control_request_t const* request) {
    UsbSetupPacket req = USBAudioBackendTinyUSB::fromTusbRequest(request);
    return USBAudioDeviceBase::activeInstance().audiod_control_xfer_cb(
        rhport, stage, req);
  };
  driver.xfer_cb = [](uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                      uint32_t xferred_bytes) {
    UsbXferResult r = (result == XFER_RESULT_SUCCESS) ? UsbXferResult::Success
                      : (result == XFER_RESULT_STALLED) ? UsbXferResult::Stalled
                                                        : UsbXferResult::Failed;
    return USBAudioDeviceBase::activeInstance().audiod_xfer_cb(
        rhport, ep_addr, r, xferred_bytes);
  };
  driver.sof = [](uint8_t rhport, uint32_t frame_count) {
    USBAudioDeviceBase::activeInstance().audiod_sof_isr(rhport, frame_count);
  };

  *count = 1;
  return &driver;
}

}  // namespace audio_tools

// Custom driver registration — routes to whichever USBAudioDeviceBase
// subclass was constructed (base or derived; set via s_active_ in the
// constructor).
//
// Deliberately NOT marked `inline`: TinyUSB's own internal default
// implementation of this function is a weak symbol, and this override must
// win at link time by being a *strong* symbol. Marking a function `inline`
// in C++ causes the compiler to emit it with weak linkage too (for ODR
// merging across translation units) -- two weak symbols racing lets the
// linker pick either one, and it was silently picking TinyUSB's built-in
// default (which registers zero class drivers), so this driver never got
// attached: every control request targeting our interfaces stalled and no
// audio ever streamed. This header is only ever included from one
// translation unit in practice (the platform-specific USB device header
// pulled in by the sketch), so plain external linkage is correct and safe.
extern "C" usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* count) {
  return audio_tools::usbAudioGetClassDriver(count);
}
