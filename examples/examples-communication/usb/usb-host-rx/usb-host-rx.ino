/**
 * @file usb-host-rx.ino (USB microphone)
 * @brief USB Audio Host: reads the audio of an attached USB microphone (or
 * headset) and measures the throughput with MeasuringStream.
 *
 * Connect the USB microphone to the USB host port of the microcontroller.
 *
 * @note for RP2040/RP2350 (arduino-pico): Tools → USB Stack: "Adafruit
 * TinyUSB Host (native)". The output is printed to Serial1 because the
 * native USB port is used as host.
 *
 * @note for ESP32-S2/S3/P4: Tools → USB Mode: "Hardware CDC and JTAG" (S3),
 * so that the USB-OTG port can be used as host. The ESP-IDF usb_host
 * library is used.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioHostStream.h"

#if defined(ARDUINO_ARCH_RP2040)
auto& MySerial = Serial1;
#else
auto& MySerial = Serial;
#endif

AudioInfo info(48000, 2, 16);
USBAudioHostStream in;
MeasuringStream out(500, &MySerial);
StreamCopy copier(out, in);

void setup() {
  MySerial.begin(115200);
  AudioToolsLogger.begin(MySerial, AudioToolsLogLevel::Info);

  // the attached device might use a different format: we get notified
  in.addNotifyAudioChange(out);
  out.begin(info);

  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  in.begin(config);
}

void loop() { copier.copy(); }
