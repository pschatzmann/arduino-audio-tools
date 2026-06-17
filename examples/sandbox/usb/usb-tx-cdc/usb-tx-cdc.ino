/**
 * @file usb-tx-cdc.ino
 * @brief USB Audio TX + USB CDC Serial on ESP32-S3 (composite device).
 *
 * The device appears to the host as both a USB microphone (UAC2) and a USB
 * serial port (CDC ACM) on the single native USB connector.
 *
 * Key trick: both interfaces must be registered with tinyusb_enable_interface()
 * BEFORE USB.begin() is called.  config.begin_usb = false tells USBAudioDevice
 * not to call USB.begin() internally, so we can add CDC first, then start USB.
 *
 * Board settings (Arduino IDE):
 *   Tools → Board            : ESP32S3 Dev Module (or your board)
 *   Tools → USB Mode         : USB-OTG (TinyUSB)
 *   Tools → USB CDC On Boot  : Disabled
 *
 * Wiring: connect the native USB port (GPIO19/20) to the host.
 * The UART port is not needed.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include <USB.h>
#include <USBCDC.h>
#include "AudioTools.h"
#include "AudioTools/Sandbox/USB/USBAudioStream.h"

USBCDC USBSerial;

AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
USBAudioStream out;
StreamCopy copier(out, sound, 80);

void setup() {
  // Register the CDC interface
  USBSerial.begin(115200);
  AudioToolsLogger.begin(USBSerial, AudioToolsLogLevel::Warning);

  // Start USB audio (TX only) with the default audio config.
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);  // registers audio + starts USB

  USB.begin(); // starts USB stack (must be called after all interfaces are registered)
  while(!USBSerial) delay(500);
  
  // Start generating the sine wave at 440 Hz (A4) with the default audio config.
  sineWave.begin(info, N_B4);

  // Show that printing worka
  USBSerial.println("USB audio + CDC started");
}

void loop() {
  copier.copy();
  out.process();
}
