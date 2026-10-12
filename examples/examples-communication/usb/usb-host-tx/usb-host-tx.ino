/**
 * @file usb-host-tx.ino (USB speaker)
 * @brief USB Audio Host: sends a sine tone to an attached USB speaker (or
 * headset).
 *
 * Connect the USB speaker to the USB host port of the microcontroller.
 *
 * @note for RP2040/RP2350 (arduino-pico): Tools → USB Stack: "Adafruit
 * TinyUSB Host (native)". The output is printed to Serial1 because the
 * native USB port is used as host.
 *
 * @note for ESP32-S2/S3/P4: Tools → USB Mode: "Hardware CDC and JTAG" (S3),
 * so that the USB-OTG port can be used as host.
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
SineWaveGenerator<int16_t> sine(16000);
GeneratedSoundStream<int16_t> sound(sine);
USBAudioHostStream out;
StreamCopy copier(out, sound);

void setup() {
  MySerial.begin(115200);
  AudioToolsLogger.begin(MySerial, AudioToolsLogLevel::Warning);

  // the attached speaker might use a different format: update the generator
  out.addNotifyAudioChange(sound);

  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // half volume on the device
  out.setVolume(0.5);

  sound.begin(info);
  sine.begin(info, N_B4);
}

void loop() { copier.copy(); }
