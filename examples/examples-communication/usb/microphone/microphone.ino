/*********************************************************************
 This example generates a sawtooth that you can output on your PC
 We use the AudioTools to generate the data input.

 We could use the callback function here as well, but we demo how
 to integrate with a (fast) Arduino Stream.

 Please read the Wiki of the project for the supported platforms!

*********************************************************************/

#include "Adafruit_TinyUSB.h" // https://github.com/pschatzmann/Adafruit_TinyUSB_Arduino
#include "AudioTools.h"  // https://github.com/pschatzmann/arduino-audio-tools

Adafruit_USBD_Audio usb;
AudioInfo info(44100, 2, 16);
SawToothGenerator<int16_t> sawtooth;               
GeneratedSoundStream<int16_t> sawthooth_stream(sawtooth);

void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  Serial.begin(115200);
  //while(!Serial);  // wait for serial

  // generate 493 hz (note B4)
  sawtooth.begin(info, 493.0f);

  // Start USB device as Audio Source
  usb.setInput(sawthooth_stream);
  usb.begin(info.sample_rate, info.channels, info.bits_per_sample);

  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  // Manual call tud_task since it isn't called by Core's background
  TinyUSBDevice.task();
  #endif
  // optional: use LED do display status
  usb.updateLED();
}