#pragma once
#include "USBDeviceAudio.h"
#include "USB.h"
#include "esp32-hal-tinyusb.h"

#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#else
#if ARDUINO_USB_MODE == 1
#error This sketch should be used when USB is in OTG mode
#endif
#endif


struct USBConfigESP32 {
  uint8_t *descr = nullptr;
  int descr_len = 0;
  int itf_count = 0;
};

static USBConfigESP32 usb_audio_config_esp32;

/// for the ESP32 the interface descriptor must be provided via a callback
uint16_t tinyusb_audio_descriptor_cb(uint8_t *dst, uint8_t *itf) {
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB Audio");
  *itf += usb_audio_config_esp32.itf_count;
  memcpy(dst, usb_audio_config_esp32.descr, usb_audio_config_esp32.descr_len);
  return usb_audio_config_esp32.descr_len;
};

/***
 * @brief ESP32 Initialization logic: We need to provide the USBAudioConfig in the
 * constructor, so that we can determine the descriptor properly when the
 * object is constructed.
 */
class USBDeviceAudioESP32 : public USBDeviceAudio {
 public:
  USBDeviceAudioESP32(USBAudioConfig config) {
    begin(config);

    int len = setupDescriptorCB();

    tinyusb_enable_interface(USB_INTERFACE_CUSTOM, len,
                             tinyusb_audio_descriptor_cb);
    // arduino_usb_event_handler_register_with(ARDUINO_USB_EVENTS,
    //                                           ARDUINO_USB_STOPPED_EVENT,
    //                                           usb_unplugged_cb, this);
    setupDebugPins();
  }

  virtual uint8_t allocInterface(uint8_t count = 1) {
    uint8_t ret = _itf_count;
    _itf_count += count;
    return ret;
  }

  virtual uint8_t allocEndpoint(uint8_t in) {
    uint8_t ret = in ? (0x80 | _epin_count++) : _epout_count++;
#if defined(ARDUINO_ARCH_ESP32) && ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    // ESP32 reserves 0x03, 0x84, 0x85 for CDC Serial
    if (ret == 0x03) {
      ret = _epout_count++;
    } else if (ret == 0x84 || ret == 0x85) {
      // Note: ESP32 does not have this much of EP IN
      _epin_count = 6;
      ret = 0x86;
    }
#endif
    return ret;
  }

 protected:
  int _itf_count = 0;
  int _epout_count = 0;
  int _epin_count = 0;

  int setupDescriptorCB() {
    int len = getInterfaceDescriptorLength(0);
    usb_audio_config_esp32.descr_len = len;
    interface_descriptor.resize(len);
    usb_audio_config_esp32.descr = interface_descriptor.data();
    usb_audio_config_esp32.itf_count = _itf_count;
    getInterfaceDescriptor(0, usb_audio_config_esp32.descr, len);
    return len;
  }

};


static USBDeviceAudioAdafruit USBAudio;
static usbd_class_driver_t audio_class_driver;

TU_ATTR_FAST_FUNC void audiod_init() { USBAudio.api().audiod_init(); }
TU_ATTR_FAST_FUNC bool audiod_deinit() { return USBAudio.api().audiod_deinit(); }
TU_ATTR_FAST_FUNC void audiod_reset(uint8_t rhport) { USBAudio.api().audiod_reset(rhport); }
TU_ATTR_FAST_FUNC uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len) {
  return USBAudio.api().audiod_open(rhport, desc_intf, max_len);
}
TU_ATTR_FAST_FUNC bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request){
    return USBAudio.api().audiod_control_xfer_cb(rhport, stage, request);
}
TU_ATTR_FAST_FUNC bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes){
    return USBAudio.api().audiod_xfer_cb(rhport, ep_addr, result, xferred_bytes);
}

TU_ATTR_FAST_FUNC void tud_audio_feedback_interval_isr(uint8_t func_id,
uint32_t frame_number, uint8_t interval_shift){
    return USBAudio.audiod_sof_isr(USBAudio.cfg.rh_port, 0);
}


// callback to register custom application driver for AUDIO
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
  audio_class_driver.name = "AUDIO";
  audio_class_driver.init = audiod_init;
  audio_class_driver.deinit = audiod_deinit;
  audio_class_driver.open  =  audiod_open;
  audio_class_driver.control_xfer_cb = audiod_control_xfer_cb;
  audio_class_driver.xfer_cb = audiod_xfer_cb;
  //audio_class_driver.sof = audiod_sof;
  return &audio_class_driver;
}

