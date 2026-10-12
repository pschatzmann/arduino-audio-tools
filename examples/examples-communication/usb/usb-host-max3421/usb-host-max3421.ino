/**
 * @file usb-host-max3421.ino
 * @brief USB Audio Host with a MAX3421E USB host controller (e.g. the
 * Adafruit USB Host FeatherWing) on any board supported by the Adafruit
 * TinyUSB Library (SAMD, nRF52, RP2040, ESP32...): reads the audio of an
 * attached USB microphone and measures the throughput.
 *
 * @note Select the Adafruit TinyUSB USB stack for your board.
 * @note The MAX3421E uses root hub port 1.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#ifndef USE_TINYUSB
#define USE_TINYUSB
#endif
#include "AudioTools.h"
#include "AudioTools/Communication/USB/USBAudioHostStream.h"

// SPI, chip select and interrupt pin of the MAX3421E
const int max3421_cs = 10;
const int max3421_int = 9;
Adafruit_USBH_Host usb_host(&SPI, max3421_cs, max3421_int);

AudioInfo info(16000, 1, 16);
USBAudioHostStream in(usb_host);
MeasuringStream out(500, &Serial);
StreamCopy copier(out, in);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  in.addNotifyAudioChange(out);
  out.begin(info);

  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  config.host_rhport = 1;  // MAX3421E
  in.begin(config);
}

void loop() { copier.copy(); }
