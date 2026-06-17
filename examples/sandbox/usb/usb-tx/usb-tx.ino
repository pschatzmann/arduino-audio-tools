/**
 * @file usb-tx.ino
 * @brief USB Audio TX using USBAudioStream — streams a sine wave to the host.
 *
 * Demonstrates the high-level USBAudioStream API in TX mode (device → host,
 * i.e. the device appears as a USB microphone / capture source).
 * USBAudioStream is a template whose default Device parameter is resolved
 * automatically by USBAudioDevice.h: USBAudioDeviceTinyUSB on RP2040 and
 * USBAudioDeviceESP32 on ESP32-S2/S3.
 *
 * Data flow:
 *   SineGenerator → GeneratedSoundStream → StreamCopy → USBAudioStream
 *   → tx_queue_ → audiod_xfer_cb → USB isochronous IN endpoint → host
 *
 * @note ESP32-S3 board setting: Tools → USB CDC On Boot → Disabled
 *   The native USB stack must be started by this sketch, not by the boot
 *   loader. If USB CDC On Boot is enabled, TinyUSB starts before setup()
 *   runs and the audio interface registration will fail with
 *   "TinyUSB has already started! Interface AUDIO not enabled".
 *   With it disabled, Serial maps to UART (Serial0) automatically.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Sandbox/USB/USBAudioStream.h"

AudioInfo info(44100, 2, 16);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
USBAudioStream out;
StreamCopy copier(out, sound, 80);

void setup(void) {
  // Use UART Serial so USB is not started before out.begin().
  // On ESP32-S3 with "USB CDC On Boot: Disabled", Serial = Serial0 (UART).
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Start generating the sine wave at 440 Hz (A4) with the default audio config.
  sineWave.begin(info, N_B4);

  // USB audio must be started before any other USB device (e.g. USB CDC).
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.begin_usb = true;
  out.begin(config);

  Serial.println("USB audio started");
}

void loop() {
  copier.copy();
  out.process();
}
