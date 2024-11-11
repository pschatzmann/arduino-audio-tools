#pragma once
#include "Adafruit_TinyUSB.h"
#include "USBDeviceAudio.h"
// Error message for Core that must select TinyUSB via menu
#if !defined(USE_TINYUSB) &&                                                   \
    (defined(ARDUINO_ARCH_SAMD) || (defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED)))
#  error TinyUSB is not selected, please select it in "Tools->Menu->USB Stack"
#endif

class USBDeviceAudioAdafruit;
USBDeviceAudioAdafruit *self_USBDeviceAudioAdafruit = nullptr;

/***
 * @brief Adafruit TinyUSB Initialization logic.
 */
class USBDeviceAudioAdafruit : public USBDeviceAudio, public Adafruit_USBD_Interface {
 public:
  USBDeviceAudioAdafruit(USBAudioConfig cfg) : USBDeviceAudio() {
    self_USBDeviceAudioAdafruit = this;
    begin(cfg);
  }

  bool begin(USBAudioConfig config) override {
    // setup config buffer
    interface_descriptor.resize(512);
    TinyUSBDevice.setConfigurationBuffer(interface_descriptor.data(), 512);

    // add string descriptor
    if (_stridx == 0) {
      _stridx = TinyUSBDevice.addStringDescriptor("TinyUSB Audio");
    }

    if (!USBDeviceAudio::begin(config)) {
      LOG_AUDIO_ERROR("begin failed");
      return false;
    }

    // add the interface
    if (!TinyUSBDevice.addInterface(*this)) {
      setStatus(AudioProcessingStatus::ERROR);
      LOG_AUDIO_ERROR("addInterface failed");
      return false;
    }
    return true;
  }

  uint8_t allocInterface(uint8_t count = 1) override {
    return TinyUSBDevice.allocInterface(count);
  }

  uint8_t allocEndpoint(uint8_t in) override {
    return TinyUSBDevice.allocEndpoint(in);
  }
  
  uint16_t getInterfaceDescriptor(uint8_t itfnum, uint8_t *buf,
                                  uint16_t bufsize) override {
    return USBDeviceAudio::getInterfaceDescriptor(itfnum, buf, bufsize);
  }
};

static usbd_class_driver_t audio_class_driver;

TU_ATTR_FAST_FUNC void audiod_init() { self_USBDeviceAudioAdafruit->api().audiod_init(); }
TU_ATTR_FAST_FUNC bool audiod_deinit() { return self_USBDeviceAudioAdafruit->api().audiod_deinit(); }
TU_ATTR_FAST_FUNC void audiod_reset(uint8_t rhport) { self_USBDeviceAudioAdafruit->api().audiod_reset(rhport); }
TU_ATTR_FAST_FUNC uint16_t audiod_open(uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len) {
  return self_USBDeviceAudioAdafruit->api().audiod_open(rhport, desc_intf, max_len);
}
TU_ATTR_FAST_FUNC bool audiod_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request){
    return self_USBDeviceAudioAdafruit->api().audiod_control_xfer_cb(rhport, stage, request);
}
TU_ATTR_FAST_FUNC bool audiod_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes){
    return self_USBDeviceAudioAdafruit->api().audiod_xfer_cb(rhport, ep_addr, result, xferred_bytes);
}

TU_ATTR_FAST_FUNC void tud_audio_feedback_interval_isr(uint8_t func_id,
uint32_t frame_number, uint8_t interval_shift){
    return self_USBDeviceAudioAdafruit->api().audiod_sof_isr(self_USBDeviceAudioAdafruit->api().config().rh_port, 0);
}


// callback to register custom application driver for AUDIO
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
  //self_USBDeviceAudioAdafruit->begin(USBConfig);
  audio_class_driver.name = "AUDIO";
  audio_class_driver.init = audiod_init;
  audio_class_driver.reset = audiod_reset;
  audio_class_driver.deinit = audiod_deinit;
  audio_class_driver.open  =  audiod_open;
  audio_class_driver.control_xfer_cb = audiod_control_xfer_cb;
  audio_class_driver.xfer_cb = audiod_xfer_cb;
  //audio_class_driver.sof = audiod_sof;
  *driver_count = 1;
  return &audio_class_driver;
}

