/**
 * @file usb-host-rx-i2s.ino
 * @brief USB Audio Host: outputs the audio of an attached USB microphone to
 * an I2S DAC.
 *
 * When the microphone can not provide the requested format, the negotiated
 * format (e.g. a different sample rate or mono) is reported to the I2S
 * output via the audio change notification.
 *
 * @note for RP2040/RP2350 (arduino-pico): Tools → USB Stack: "Adafruit
 * TinyUSB Host (native)".
 *
 * @note for ESP32-S2/S3/P4: Tools → USB Mode: "Hardware CDC and JTAG" (S3).
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioHostStream.h"

AudioInfo info(48000, 2, 16);
USBAudioHostStream mic;
I2SStream i2s;
StreamCopy copier(i2s, mic);

void setup() {
  // the I2S output follows the format of the attached microphone
  mic.addNotifyAudioChange(i2s);

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  i2s.begin(cfg);

  auto config = mic.defaultConfig(RX_MODE);
  config.copyFrom(info);
  config.host_apply_device_format = true;
  mic.begin(config);
}

void loop() { copier.copy(); }
