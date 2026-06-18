/**
 * @file usb-rx-cdc.ino
 * @brief USB Audio RX + USB CDC Serial on ESP32-S3 (composite device).
 *
 * The device appears to the host as both a USB speaker (UAC2 RX — host sends
 * audio to the device) and a USB serial port (CDC ACM) on the single native
 * USB connector.  Audio throughput is measured and printed to the CDC serial
 * port via MeasuringStream.
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
#include "AudioTools.h"
#include "AudioTools/Sandbox/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
USBAudioStream in;
MeasuringStream out(200, &Serial);
StreamCopy copier(out, in, 80);

void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  // Register the CDC interface first (gets interfaces 0-1).
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Start MeasuringStream so it knows the audio format.
  out.begin(info);

  // Register USB audio in RX mode (host → device, i.e. USB speaker).
  // begin_usb defaults to false so USB.begin() below controls the start.
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  // Disable the explicit-feedback endpoint for now: Linux's snd-usb-audio
  // handles synchronous isochronous OUT reliably without it, and the simpler
  // path avoids activation failures seen with the feedback EP on ESP32-S3.
  config.enable_feedback_ep = false;
  in.begin(config);

  // Start the USB stack after all interfaces (CDC + Audio) are registered.
  USB.begin();
  while (!Serial) delay(500);

  Serial.println("USB audio RX + CDC started");

  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void loop() {
  in.process();   // drain ep_out_ff → rx_queue_
  copier.copy();  // read rx_queue_ → MeasuringStream
}
