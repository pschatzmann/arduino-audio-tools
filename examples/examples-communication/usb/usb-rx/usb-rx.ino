/**
 * @file usb-rx.ino (speaker)
 * @brief USB Audio RX + USB CDC Serial on ESP32-S3 (composite device).
 *
 * The device appears to the host as a USB speaker (UAC2 RX — host sends
 * audio to the device). Audio throughput is measured and printed Serial via
 * MeasuringStream.
 *
 * @note for ESP32: Board settings (Arduino IDE):
 *   Tools → Board            : ESP32S3 Dev Module (or your board)
 *   Tools → USB Mode         : USB-OTG (TinyUSB)
 *   Tools → USB CDC On Boot  : Disabled
 *   If you want to use the CDC serial port for logging: define USBCDC
 * USBSerial; and use USBSerial!
 * 
 * @note for boards using Adafruit TinyUSB core (e.g. RP2040, STM32, SAMD):
 * Board settings (Arduino IDE): USB Mode: USB OTG (TinyUSB)
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioStream.h"

// CDC output is pltform dependent
#ifdef ESP32
USBCDC MySerial;
#else
auto& MySerial = Serial;
#endif

AudioInfo info(44100, 2, 16);
USBAudioStream in;
MeasuringStream out(500, &MySerial);
StreamCopy copier(out, in, 80);


void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed
  // rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  // Register the CDC interface first (gets interfaces 0-1).
  MySerial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Start MeasuringStream so it knows the audio format.
  out.begin(info);

  // Register USB audio in RX mode (host → device, i.e. USB speaker).
  // begin_usb defaults to false so USB.begin() below controls the start.
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  in.begin(config);

  // If already enumerated, additional class driver begin() e.g msc, hid, midi
  // won't take effect until re-enumeration: on ESP32 you can alternatively call USB.begin()
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  MySerial.println("USB audio RX + CDC started");
}

void loop() {
  copier.copy();  // read ep_out_ff → MeasuringStream
}
