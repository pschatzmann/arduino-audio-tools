#pragma once
#define CFG_TUSB_DEBUG_PRINTF 3
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
  USBDeviceAudioAdafruit() : USBDeviceAudio() {
    self_USBDeviceAudioAdafruit = this;
  }

  bool begin(USBAudioConfig config) override {
    // setup config buffer
    if (interface_descriptor.size() != 512){
      interface_descriptor.resize(512);
      TinyUSBDevice.setConfigurationBuffer(interface_descriptor.data(), 512);
    }

    // add string descriptor
    if (_stridx == 0) {
      _stridx = TinyUSBDevice.addStringDescriptor("TinyUSB Audio");
    }

    if (!USBDeviceAudio::begin(config)) {
      LOG_AUDIO_ERROR("begin failed");
      return false;
    }

    //  add the interface
    if (!TinyUSBDevice.addInterface(*self_USBDeviceAudioAdafruit)) {
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
    return USBDeviceAudio::getInterfaceDescriptor(buf, bufsize);
  }
};


// callback to register custom application driver for AUDIO
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
  static usbd_class_driver_t audio_class_driver;
  audio_class_driver.name = "AUDIO";
  audio_class_driver.init =  []() { 
    self_USBDeviceAudioAdafruit->begin(self_USBDeviceAudioAdafruit->api().config());
    self_USBDeviceAudioAdafruit->api().audiod_init(); 
  };
  audio_class_driver.deinit = []() { return self_USBDeviceAudioAdafruit->api().audiod_deinit(); };
  audio_class_driver.reset = [](uint8_t rhport) { self_USBDeviceAudioAdafruit->api().audiod_reset(rhport); };
  audio_class_driver.open  =  [](uint8_t rhport, tusb_desc_interface_t const * desc_intf, uint16_t max_len) {
    return self_USBDeviceAudioAdafruit->api().audiod_open(rhport, desc_intf, max_len);
  };
  audio_class_driver.control_xfer_cb = [](uint8_t rhport, uint8_t stage, tusb_control_request_t const * request){
     return self_USBDeviceAudioAdafruit->api().audiod_control_xfer_cb(rhport, stage, request);
  };
  audio_class_driver.xfer_cb = [](uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes){
    return self_USBDeviceAudioAdafruit->api().audiod_xfer_cb(rhport, ep_addr, result, xferred_bytes);
  };
  audio_class_driver.sof = [](uint8_t rhport, uint32_t frame_count) { self_USBDeviceAudioAdafruit->api().audiod_sof_isr(self_USBDeviceAudioAdafruit->api().config().rh_port, 0); };
 

 
  *driver_count = 1;
  return &audio_class_driver;
}

