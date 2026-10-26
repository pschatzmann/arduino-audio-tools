#pragma once

#include <cstring>
#include <vector>

#include "AudioLogger.h"
#include "AudioTools/Communication/USB/USBAudioConstants.h"
#include "AudioTools/Communication/USB/USBAudioHostBackend.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

namespace audio_tools {

/**
 * @brief USBAudioHostBackend for the ESP-IDF usb_host library which is part
 * of arduino-esp32 (ESP32-S2, ESP32-S3 and ESP32-P4).
 *
 * begin() installs the USB host library (unless it was already installed,
 * e.g. by another library) and starts a task which processes the library
 * events. The client events and transfer completions are processed by
 * task(), which is called continuously by the USB task of
 * USBAudioHostESP32.
 *
 * Isochronous endpoints are streamed with two transfers in flight, each
 * covering kPacketsPerTransfer service intervals.
 *
 * @ingroup usb
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class USBAudioHostBackendESP32 : public USBAudioHostBackend {
 public:
  /// Number of isochronous packets (1 ms at full speed) per transfer
  static constexpr int kPacketsPerTransfer = 4;
  /// Number of transfers which are kept in flight per endpoint
  static constexpr int kTransfersInFlight = 2;

  ~USBAudioHostBackendESP32() {
    // the USB host library stays installed: other clients might use it
    if (client_ != nullptr) {
      closeDevice();
      usb_host_client_deregister(client_);
      client_ = nullptr;
    }
  }

  bool begin(USBAudioHostEvents* events, const USBAudioConfig& cfg) override {
    events_ = events;
    (void)cfg;
    if (!installLibrary()) return false;
    if (client_ == nullptr) {
      usb_host_client_config_t client_cfg;
      memset(&client_cfg, 0, sizeof(client_cfg));
      client_cfg.is_synchronous = false;
      client_cfg.max_num_event_msg = 5;
      client_cfg.async.client_event_callback = clientEvent;
      client_cfg.async.callback_arg = this;
      esp_err_t rc = usb_host_client_register(&client_cfg, &client_);
      if (rc != ESP_OK) {
        LOGE("usb_host_client_register: %s", esp_err_to_name(rc));
        client_ = nullptr;
        return false;
      }
    }
    if (ctrl_xfer_ == nullptr) {
      esp_err_t rc =
          usb_host_transfer_alloc(kCtrlXferSize, 0, &ctrl_xfer_);
      if (rc != ESP_OK) {
        LOGE("usb_host_transfer_alloc: %s", esp_err_to_name(rc));
        return false;
      }
    }
    return true;
  }

  void end() override {
    for (auto& s : streams_) {
      if (s.open) closeStream(daddr_, s.ep_addr);
    }
  }

  void task() override {
    if (client_ == nullptr) {
      vTaskDelay(1);
      return;
    }
    // processes client events and transfer completions (callbacks)
    usb_host_client_handle_events(client_, 1);
  }

  bool control(uint8_t daddr, const UsbSetupPacket& setup,
               uint8_t* data) override {
    if (dev_ == nullptr || daddr != daddr_ || ctrl_xfer_ == nullptr)
      return false;
    if (setup.wLength + 8 > kCtrlXferSize) return false;
    usb_setup_packet_t* pkt = (usb_setup_packet_t*)ctrl_xfer_->data_buffer;
    pkt->bmRequestType = setup.bmRequestType;
    pkt->bRequest = setup.bRequest;
    pkt->wValue = setup.wValue;
    pkt->wIndex = setup.wIndex;
    pkt->wLength = setup.wLength;
    bool is_in = (setup.bmRequestType & 0x80) != 0;
    if (!is_in && setup.wLength > 0 && data != nullptr)
      memcpy(ctrl_xfer_->data_buffer + 8, data, setup.wLength);
    ctrl_xfer_->num_bytes = 8 + setup.wLength;
    ctrl_xfer_->device_handle = dev_;
    ctrl_xfer_->bEndpointAddress = 0;
    ctrl_xfer_->callback = controlComplete;
    ctrl_xfer_->context = this;
    ctrl_xfer_->timeout_ms = 1000;
    ctrl_data_ = is_in ? data : nullptr;
    ctrl_len_ = setup.wLength;
    esp_err_t rc = usb_host_transfer_submit_control(client_, ctrl_xfer_);
    if (rc != ESP_OK) {
      LOGW("usb_host_transfer_submit_control: %s", esp_err_to_name(rc));
      return false;
    }
    return true;
  }

  /// Claims the interface with the alternate setting (which allocates its
  /// endpoints) and sends SET_INTERFACE.
  bool setInterface(uint8_t daddr, uint8_t itf, uint8_t alt) override {
    if (dev_ == nullptr || daddr != daddr_) return false;
    Interface* claimed = findInterface(itf);
    if (claimed != nullptr && claimed->claimed && claimed->alt != alt) {
      if (!releaseInterface(*claimed)) return false;  // retry later
    }
    if (alt != 0 && (claimed == nullptr || !claimed->claimed)) {
      esp_err_t rc = usb_host_interface_claim(client_, dev_, itf, alt);
      if (rc != ESP_OK) {
        LOGE("usb_host_interface_claim %d/%d: %s", itf, alt,
             esp_err_to_name(rc));
        return false;
      }
      if (claimed == nullptr) {
        interfaces_.push_back(Interface());
        claimed = &interfaces_.back();
      }
      claimed->itf = itf;
      claimed->alt = alt;
      claimed->claimed = true;
    }
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
    if (dev_ == nullptr || daddr != daddr_) return false;
    Stream* s = findStream(ep.bEndpointAddress);
    if (s == nullptr) {
      streams_.push_back(Stream());
      s = &streams_.back();
    }
    if (s->in_flight > 0) {
      LOGE("Endpoint 0x%x is still busy", ep.bEndpointAddress);
      return false;
    }
    freeTransfers(*s);
    s->ep_addr = ep.bEndpointAddress;
    s->role = role;
    s->max_packet = ep.packetSize();
    s->packets = role == UsbHostStreamRole::Feedback ? 1 : kPacketsPerTransfer;
    for (int j = 0; j < kTransfersInFlight; j++) {
      usb_transfer_t* xfer = nullptr;
      esp_err_t rc = usb_host_transfer_alloc(
          (size_t)s->max_packet * s->packets, s->packets, &xfer);
      if (rc != ESP_OK) {
        LOGE("usb_host_transfer_alloc: %s", esp_err_to_name(rc));
        freeTransfers(*s);
        return false;
      }
      xfer->device_handle = dev_;
      xfer->bEndpointAddress = ep.bEndpointAddress;
      xfer->callback = isoComplete;
      xfer->context = this;
      s->transfers.push_back(xfer);
    }
    s->open = true;
    for (auto xfer : s->transfers) submit(*s, xfer);
    return true;
  }

  void closeStream(uint8_t daddr, uint8_t ep_addr) override {
    (void)daddr;
    Stream* s = findStream(ep_addr);
    if (s == nullptr || !s->open) return;
    s->open = false;
    if (dev_ == nullptr) return;
    // cancels the transfers in flight: they are returned via isoComplete()
    usb_host_endpoint_halt(dev_, ep_addr);
    usb_host_endpoint_flush(dev_, ep_addr);
    usb_host_endpoint_clear(dev_, ep_addr);
  }

  UsbSpeed speed() const override {
    if (dev_ == nullptr) return UsbSpeed::Full;
    usb_device_info_t info;
    if (usb_host_device_info(dev_, &info) != ESP_OK) return UsbSpeed::Full;
    if (info.speed == USB_SPEED_HIGH) return UsbSpeed::High;
    if (info.speed == USB_SPEED_LOW) return UsbSpeed::Low;
    return UsbSpeed::Full;
  }

  uint32_t millis() const override {
    return (uint32_t)(esp_timer_get_time() / 1000);
  }

 protected:
  static constexpr size_t kCtrlXferSize = 8 + 512;

  struct Stream {
    uint8_t ep_addr = 0;
    UsbHostStreamRole role = UsbHostStreamRole::RxData;
    uint16_t max_packet = 0;
    int packets = 1;
    bool open = false;
    int in_flight = 0;
    std::vector<usb_transfer_t*> transfers;
  };

  struct Interface {
    uint8_t itf = 0;
    uint8_t alt = 0;
    bool claimed = false;
  };

  inline static bool s_library_installed_ = false;
  inline static TaskHandle_t s_library_task_ = nullptr;
  USBAudioHostEvents* events_ = nullptr;
  usb_host_client_handle_t client_ = nullptr;
  usb_device_handle_t dev_ = nullptr;
  uint8_t daddr_ = 0;
  usb_transfer_t* ctrl_xfer_ = nullptr;
  uint8_t* ctrl_data_ = nullptr;
  uint16_t ctrl_len_ = 0;
  std::vector<Stream> streams_;
  std::vector<Interface> interfaces_;

  /// Installs the USB host library and starts the task which handles its
  /// events
  static bool installLibrary() {
    if (s_library_installed_) return true;
    usb_host_config_t host_cfg;
    memset(&host_cfg, 0, sizeof(host_cfg));
    host_cfg.skip_phy_setup = false;
    host_cfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t rc = usb_host_install(&host_cfg);
    if (rc == ESP_ERR_INVALID_STATE) {
      LOGI("USB host library is already installed");
    } else if (rc != ESP_OK) {
      LOGE("usb_host_install: %s", esp_err_to_name(rc));
      return false;
    } else {
      xTaskCreate(libraryTask, "usb_host_lib", 4096, nullptr, 5,
                  &s_library_task_);
    }
    s_library_installed_ = true;
    return true;
  }

  static void libraryTask(void*) {
    while (true) {
      uint32_t flags = 0;
      usb_host_lib_handle_events(portMAX_DELAY, &flags);
      if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
        usb_host_device_free_all();
      }
    }
  }

  static void clientEvent(const usb_host_client_event_msg_t* msg, void* arg) {
    USBAudioHostBackendESP32* self = (USBAudioHostBackendESP32*)arg;
    switch (msg->event) {
      case USB_HOST_CLIENT_EVENT_NEW_DEV:
        self->openDevice(msg->new_dev.address);
        break;
      case USB_HOST_CLIENT_EVENT_DEV_GONE:
        if (msg->dev_gone.dev_hdl == self->dev_) {
          uint8_t daddr = self->daddr_;
          self->closeDevice();
          if (self->events_) self->events_->onDetach(daddr);
        }
        break;
      default:
        break;
    }
  }

  /// Opens a newly enumerated device and provides its audio control
  /// interface to the class logic
  void openDevice(uint8_t address) {
    if (dev_ != nullptr || events_ == nullptr) return;
    usb_device_handle_t dev = nullptr;
    if (usb_host_device_open(client_, address, &dev) != ESP_OK) return;
    const usb_device_desc_t* dev_desc = nullptr;
    const usb_config_desc_t* cfg_desc = nullptr;
    if (usb_host_get_device_descriptor(dev, &dev_desc) != ESP_OK ||
        usb_host_get_active_config_descriptor(dev, &cfg_desc) != ESP_OK) {
      usb_host_device_close(client_, dev);
      return;
    }
    const uint8_t* start = (const uint8_t*)cfg_desc;
    const uint8_t* end = start + cfg_desc->wTotalLength;
    // the class logic sends control requests to dev_
    dev_ = dev;
    daddr_ = address;
    bool accepted = false;
    for (const uint8_t* p = start; p + 2 <= end && p[0] >= 2; p += p[0]) {
      if (p[1] != kUsbDescTypeInterface || p[0] < 9) continue;
      if (p[5] != kUsbClassAudio || p[6] != kAudioSubclassControl) continue;
      uint16_t len = events_->onAttach(address, dev_desc->idVendor,
                                       dev_desc->idProduct, p,
                                       (uint16_t)(end - p));
      if (len > 0) {
        accepted = true;
        break;
      }
    }
    if (!accepted) {
      dev_ = nullptr;
      daddr_ = 0;
      usb_host_device_close(client_, dev);
      return;
    }
    // ESP-IDF has already selected the configuration
    events_->onConfigured(address);
  }

  void closeDevice() {
    if (dev_ == nullptr) return;
    for (auto& s : streams_) {
      if (s.open) closeStream(daddr_, s.ep_addr);
    }
    for (auto& itf : interfaces_) {
      if (itf.claimed) usb_host_interface_release(client_, dev_, itf.itf);
      itf.claimed = false;
    }
    for (auto& s : streams_) {
      if (s.in_flight == 0) freeTransfers(s);
    }
    usb_host_device_close(client_, dev_);
    dev_ = nullptr;
    daddr_ = 0;
  }

  Stream* findStream(uint8_t ep_addr) {
    for (auto& s : streams_) {
      if (s.ep_addr == ep_addr) return &s;
    }
    return nullptr;
  }

  Stream* findStream(usb_transfer_t* xfer) {
    for (auto& s : streams_) {
      for (auto t : s.transfers) {
        if (t == xfer) return &s;
      }
    }
    return nullptr;
  }

  Interface* findInterface(uint8_t itf) {
    for (auto& i : interfaces_) {
      if (i.itf == itf) return &i;
    }
    return nullptr;
  }

  /// Releasing an interface requires that its endpoints are idle: when
  /// transfers are still being returned this fails and setInterface() is
  /// retried by the class logic.
  bool releaseInterface(Interface& itf) {
    esp_err_t rc = usb_host_interface_release(client_, dev_, itf.itf);
    if (rc != ESP_OK) {
      LOGI("usb_host_interface_release %d: %s", itf.itf, esp_err_to_name(rc));
      return false;
    }
    itf.claimed = false;
    return true;
  }

  void freeTransfers(Stream& s) {
    for (auto t : s.transfers) usb_host_transfer_free(t);
    s.transfers.clear();
  }

  void submit(Stream& s, usb_transfer_t* xfer) {
    int total = 0;
    for (int j = 0; j < s.packets; j++) {
      int len = s.max_packet;
      if (s.role == UsbHostStreamRole::TxData) {
        len = events_ ? events_->onIsoOut(xfer->data_buffer + total,
                                          s.max_packet)
                      : 0;
      }
      xfer->isoc_packet_desc[j].num_bytes = len;
      total += len;
    }
    xfer->num_bytes = total;
    if (usb_host_transfer_submit(xfer) == ESP_OK) {
      s.in_flight++;
    } else {
      LOGW("usb_host_transfer_submit 0x%x failed", s.ep_addr);
    }
  }

  static void isoComplete(usb_transfer_t* xfer) {
    USBAudioHostBackendESP32* self = (USBAudioHostBackendESP32*)xfer->context;
    Stream* s = self->findStream(xfer);
    if (s == nullptr) return;
    s->in_flight--;
    if (!s->open || self->dev_ == nullptr) {
      if (s->in_flight == 0 && self->dev_ == nullptr) self->freeTransfers(*s);
      return;
    }
    if (s->role != UsbHostStreamRole::TxData && self->events_ &&
        xfer->status == USB_TRANSFER_STATUS_COMPLETED) {
      // IN: packet j starts at j * max_packet
      for (int j = 0; j < s->packets; j++) {
        usb_isoc_packet_desc_t& pd = xfer->isoc_packet_desc[j];
        if (pd.status != USB_TRANSFER_STATUS_COMPLETED) continue;
        self->events_->onIsoIn(s->role, xfer->data_buffer + j * s->max_packet,
                               (uint16_t)pd.actual_num_bytes);
      }
    }
    if (xfer->status == USB_TRANSFER_STATUS_NO_DEVICE) return;
    self->submit(*s, xfer);
  }

  static void controlComplete(usb_transfer_t* xfer) {
    USBAudioHostBackendESP32* self = (USBAudioHostBackendESP32*)xfer->context;
    bool ok = xfer->status == USB_TRANSFER_STATUS_COMPLETED;
    uint16_t len = 0;
    if (ok && xfer->actual_num_bytes > 8) {
      len = (uint16_t)(xfer->actual_num_bytes - 8);
      if (len > self->ctrl_len_) len = self->ctrl_len_;
      if (self->ctrl_data_) memcpy(self->ctrl_data_, xfer->data_buffer + 8, len);
    }
    if (self->events_) self->events_->onControlComplete(ok, len);
  }
};

}  // namespace audio_tools
