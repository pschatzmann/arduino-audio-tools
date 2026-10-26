#pragma once
#include <Adafruit_TinyUSB.h>

#include <cstring>
#include <vector>

#include "AudioTools/Communication/USB/USBAudioConstants.h"
#include "AudioTools/Communication/USB/USBAudioHostBackend.h"

extern "C" {
#include "host/hcd.h"
#include "host/usbh.h"
#include "host/usbh_pvt.h"
#include "tusb.h"
}

#if !CFG_TUH_ENABLED
#error "USB Audio Host: host support is not enabled in the Adafruit TinyUSB Library (CFG_TUH_ENABLED); e.g. on RP2040 select the USB Stack 'Adafruit TinyUSB Host'"
#endif

#if TUSB_VERSION_NUMBER < 1700
#error "USB Audio Host requires TinyUSB >= 0.17 (a current Adafruit TinyUSB Library)"
#endif

namespace audio_tools {

/**
 * @brief USBAudioHostBackend for the TinyUSB host stack provided by the
 * Adafruit TinyUSB Library (RP2040/RP2350 native or Pico-PIO-USB, ESP32-S2/
 * S3/P4 native OTG, STM32, or a MAX3421E host controller on any platform).
 *
 * TinyUSB does not provide a host side Audio class driver, so this backend
 * registers its own usbh_class_driver_t via usbh_app_driver_get_cb() (only
 * one application host driver can be registered by a sketch).
 *
 * The isochronous transfers are resubmitted at most once per USB frame:
 * some host controller drivers (e.g. the RP2040 native one) do not schedule
 * isochronous transfers per frame, so this keeps the packet rate at the
 * nominal 1 kHz.
 *
 * @note TinyUSB reads the whole configuration descriptor into a buffer of
 * CFG_TUH_ENUMERATION_BUFSIZE bytes (256 in the Adafruit library): devices
 * with a larger configuration descriptor (e.g. many headsets) are rejected
 * by the enumeration. Increase this value in the tusb_config_<mcu>.h of
 * the Adafruit TinyUSB Library if your device is not detected.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostBackendTinyUSB : public USBAudioHostBackend {
 public:
  USBAudioHostBackendTinyUSB() = default;

  /// Use a Adafruit_USBH_Host which was set up by the sketch (e.g. for a
  /// MAX3421E or Pico-PIO-USB)
  USBAudioHostBackendTinyUSB(Adafruit_USBH_Host& host) : p_host_(&host) {}

  /// Defines the Adafruit_USBH_Host (call before begin())
  void setHost(Adafruit_USBH_Host& host) { p_host_ = &host; }

  bool begin(USBAudioHostEvents* events, const USBAudioConfig& cfg) override {
    events_ = events;
    s_instance_ = this;
    rhport_ = cfg.host_rhport;
#if CFG_TUH_ENUMERATION_BUFSIZE < 512
    LOGW(
        "CFG_TUH_ENUMERATION_BUFSIZE is %d: devices with a larger "
        "configuration descriptor are not detected",
        (int)CFG_TUH_ENUMERATION_BUFSIZE);
#endif
    if (tuh_rhport_is_active(rhport_)) return true;
    return host().begin(rhport_);
  }

  void end() override {
    for (auto& s : streams_) {
      if (s.open) closeStream(s.daddr, s.ep_addr);
    }
  }

  void task() override {
    host().task(0, false);
    // resubmit the transfers which were deferred to the next frame
    for (auto& s : streams_) {
      if (s.open && s.due && frameAdvanced(s)) submit(s);
    }
  }

  bool control(uint8_t daddr, const UsbSetupPacket& setup,
               uint8_t* data) override {
    request_.bmRequestType = setup.bmRequestType;
    request_.bRequest = setup.bRequest;
    request_.wValue = tu_htole16(setup.wValue);
    request_.wIndex = tu_htole16(setup.wIndex);
    request_.wLength = tu_htole16(setup.wLength);
    tuh_xfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.daddr = daddr;
    xfer.ep_addr = 0;
    xfer.setup = &request_;
    xfer.buffer = setup.wLength > 0 ? data : nullptr;
    xfer.complete_cb = controlComplete;
    xfer.user_data = 0;
    return tuh_control_xfer(&xfer);
  }

  bool setInterface(uint8_t daddr, uint8_t itf, uint8_t alt) override {
    UsbSetupPacket setup;
    setup.bmRequestType = 0x01;  // standard, host to device, interface
    setup.bRequest = kUsbReqSetInterface;
    setup.wValue = alt;
    setup.wIndex = itf;
    setup.wLength = 0;
    return control(daddr, setup, nullptr);
  }

  bool openStream(uint8_t daddr, const UsbEndpointDescriptorView& ep,
                  UsbHostStreamRole role) override {
    tusb_desc_endpoint_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.bLength = sizeof(tusb_desc_endpoint_t);
    desc.bDescriptorType = TUSB_DESC_ENDPOINT;
    desc.bEndpointAddress = ep.bEndpointAddress;
    desc.bmAttributes.xfer = TUSB_XFER_ISOCHRONOUS;
    desc.bmAttributes.sync = ep.sync;
    desc.bmAttributes.usage = ep.usage;
    desc.wMaxPacketSize = tu_htole16(ep.wMaxPacketSize);
    desc.bInterval = ep.bInterval;
    if (!tuh_edpt_open(daddr, &desc)) {
      LOGE("tuh_edpt_open 0x%x failed", ep.bEndpointAddress);
      return false;
    }
    Stream* s = find(daddr, ep.bEndpointAddress);
    if (s == nullptr) {
      streams_.push_back(Stream());
      s = &streams_.back();
    }
    s->daddr = daddr;
    s->ep_addr = ep.bEndpointAddress;
    s->role = role;
    s->max_packet = ep.packetSize();
    s->buffer.resize(s->max_packet);
    s->open = true;
    s->due = false;
    s->last_frame = frameNumber() - 1;
    submit(*s);
    return true;
  }

  void closeStream(uint8_t daddr, uint8_t ep_addr) override {
    Stream* s = find(daddr, ep_addr);
    if (s == nullptr || !s->open) return;
    s->open = false;
    s->due = false;
    tuh_edpt_abort_xfer(daddr, ep_addr);
    tuh_edpt_close(daddr, ep_addr);
  }

  UsbSpeed speed() const override {
    tusb_speed_t sp = tuh_speed_get(daddr_);
    if (sp == TUSB_SPEED_HIGH) return UsbSpeed::High;
    if (sp == TUSB_SPEED_LOW) return UsbSpeed::Low;
    return UsbSpeed::Full;
  }

  uint32_t millis() const override { return tusb_time_millis_api(); }

  // ── usbh_class_driver_t trampolines ──────────────────────────────────────

  static bool driverInit() { return true; }

  static bool driverDeinit() { return true; }

  static uint16_t driverOpen(uint8_t rhport, uint8_t daddr,
                             tusb_desc_interface_t const* itf_desc,
                             uint16_t max_len) {
    USBAudioHostBackendTinyUSB* self = s_instance_;
    if (self == nullptr || self->events_ == nullptr) return 0;
    if (rhport != self->rhport_) return 0;
    uint16_t vid = 0, pid = 0;
    tuh_vid_pid_get(daddr, &vid, &pid);
    const uint8_t* desc = (const uint8_t*)itf_desc;
    uint16_t len = self->events_->onAttach(daddr, vid, pid, desc, max_len);
    if (len < sizeof(tusb_desc_interface_t)) return 0;
    // remember the last interface which belongs to the audio function
    self->daddr_ = daddr;
    self->last_itf_ = itf_desc->bInterfaceNumber;
    for (const uint8_t* p = desc; p < desc + len && p[0] > 0; p += p[0]) {
      if (p[1] == TUSB_DESC_INTERFACE && p[2] > self->last_itf_)
        self->last_itf_ = p[2];
    }
    return len;
  }

  static bool driverSetConfig(uint8_t daddr, uint8_t itf_num) {
    USBAudioHostBackendTinyUSB* self = s_instance_;
    if (self && self->events_ && daddr == self->daddr_) {
      self->events_->onConfigured(daddr);
      usbh_driver_set_config_complete(daddr, self->last_itf_);
    } else {
      usbh_driver_set_config_complete(daddr, itf_num);
    }
    return true;
  }

  static bool driverXfer(uint8_t daddr, uint8_t ep_addr, xfer_result_t result,
                         uint32_t xferred_bytes) {
    USBAudioHostBackendTinyUSB* self = s_instance_;
    if (self == nullptr) return false;
    Stream* s = self->find(daddr, ep_addr);
    if (s == nullptr || !s->open) return true;
    if (result == XFER_RESULT_SUCCESS && s->role != UsbHostStreamRole::TxData &&
        self->events_) {
      uint16_t len = xferred_bytes > s->max_packet ? s->max_packet
                                                   : (uint16_t)xferred_bytes;
      self->events_->onIsoIn(s->role, s->buffer.data(), len);
    }
    if (!s->open) return true;  // closed by the callback
    if (self->frameAdvanced(*s)) {
      self->submit(*s);
    } else {
      s->due = true;
    }
    return true;
  }

  static void driverClose(uint8_t daddr) {
    USBAudioHostBackendTinyUSB* self = s_instance_;
    if (self == nullptr) return;
    for (auto& s : self->streams_) {
      if (s.daddr == daddr) {
        s.open = false;
        s.due = false;
      }
    }
    if (daddr == self->daddr_) self->daddr_ = 0;
    if (self->events_) self->events_->onDetach(daddr);
  }

 protected:
  struct Stream {
    uint8_t daddr = 0;
    uint8_t ep_addr = 0;
    UsbHostStreamRole role = UsbHostStreamRole::RxData;
    uint16_t max_packet = 0;
    std::vector<uint8_t> buffer;
    bool open = false;
    bool due = false;
    uint32_t last_frame = 0;
  };

  inline static USBAudioHostBackendTinyUSB* s_instance_ = nullptr;
  Adafruit_USBH_Host* p_host_ = nullptr;
  USBAudioHostEvents* events_ = nullptr;
  uint8_t rhport_ = 0;
  uint8_t daddr_ = 0;
  uint8_t last_itf_ = 0;
  tusb_control_request_t request_;
  std::vector<Stream> streams_;

  Adafruit_USBH_Host& host() {
    if (p_host_ == nullptr) {
      static Adafruit_USBH_Host default_host;
      p_host_ = &default_host;
    }
    return *p_host_;
  }

  Stream* find(uint8_t daddr, uint8_t ep_addr) {
    for (auto& s : streams_) {
      if (s.daddr == daddr && s.ep_addr == ep_addr) return &s;
    }
    return nullptr;
  }

  uint32_t frameNumber() { return hcd_frame_number(rhport_); }

  bool frameAdvanced(const Stream& s) { return frameNumber() != s.last_frame; }

  void submit(Stream& s) {
    s.due = false;
    s.last_frame = frameNumber();
    uint16_t len = s.max_packet;
    if (s.role == UsbHostStreamRole::TxData) {
      len = events_ ? events_->onIsoOut(s.buffer.data(), s.max_packet) : 0;
    }
    if (!usbh_edpt_xfer(s.daddr, s.ep_addr, s.buffer.data(), len)) {
      // e.g. still busy: try again with the next frame
      s.due = true;
    }
  }

  static void controlComplete(tuh_xfer_t* xfer) {
    USBAudioHostBackendTinyUSB* self = s_instance_;
    if (self == nullptr || self->events_ == nullptr) return;
    self->events_->onControlComplete(xfer->result == XFER_RESULT_SUCCESS,
                                     (uint16_t)xfer->actual_len);
  }
};

}  // namespace audio_tools

// Custom host class driver registration. Deliberately NOT `inline`: it must
// be a strong symbol which overrides TinyUSB's weak default (see the same
// comment for usbd_app_driver_get_cb() in USBAudioBackendTinyUSB.h).
extern "C" usbh_class_driver_t const* usbh_app_driver_get_cb(
    uint8_t* driver_count) {
  using audio_tools::USBAudioHostBackendTinyUSB;
  static usbh_class_driver_t const driver = {
      "AUDIO",
      USBAudioHostBackendTinyUSB::driverInit,
      USBAudioHostBackendTinyUSB::driverDeinit,
      USBAudioHostBackendTinyUSB::driverOpen,
      USBAudioHostBackendTinyUSB::driverSetConfig,
      USBAudioHostBackendTinyUSB::driverXfer,
      USBAudioHostBackendTinyUSB::driverClose,
  };
  *driver_count = 1;
  return &driver;
}
