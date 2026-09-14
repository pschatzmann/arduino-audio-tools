/**
 * @file usb-rx-i2s.ino (speaker)
 * @brief USB Audio RX (host → device) forwarded directly to I2S output.
 *
 * The device appears to the host as a USB speaker (UAC2 RX). Instead of
 * copying data out in loop(), the audio is pushed to I2S from the
 * rxDoneCallback as soon as a USB packet arrives, avoiding an extra buffer
 * copy. The I2S output's availableForWrite() is reported back to the host
 * as a feedback percentage so the host can pace its sample rate to match
 * the I2S consumer.
 *
 * Data flow:
 *   Host → USB isochronous OUT endpoint → USBAudioStream.bufferRx()
 *   → rxDone() → I2SStream.write()
 *
 * @note for ESP32: Board settings (Arduino IDE):
 *   Tools → Board            : ESP32S3 Dev Module (or your board)
 *   Tools → USB Mode         : USB-OTG (TinyUSB)
 *   Tools → USB CDC On Boot  : Disabled
 *   If you want to use the CDC serial port for logging: define USBCDC
 * MySerial; and use MySerial!
 *
 * @note for boards using Adafruit TinyUSB core (e.g. RP2040, STM32, SAMD):
 * Board settings (Arduino IDE): USB Mode: USB OTG (TinyUSB)
 *
 * @note This example only works for platforms where the i2s availableForWrite()
 * is a measure of the number of bytes that can be written to the I2S output
 * buffer. This is true for ESP32 and RP2040, but not for all platforms.
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

AudioInfo info(48000, 2, 16);
USBAudioStream in;
I2SStream out;

// diagnostics: total capacity of the I2S output buffer, so we can print
// out.availableForWrite() as a percentage (see setup()/loop())
size_t i2s_buffer_capacity = 0;

// Callback to push data to I2S
bool rxDone(USBAudioDeviceBase* p_usb, uint8_t,
            USBAudioDeviceBase::audiod_function_t*, uint16_t) {
  // read Audio data
  int len = in.bufferRx().available();
  uint8_t data[len];
  in.bufferRx().readArray(data, len);

  // write to I2S
  out.write(data, len);

  // update feedback
  int i2s_free_pct = 100 * out.availableForWrite() / (int)i2s_buffer_capacity;
  in.setFeedbackPercent(i2s_free_pct);
  return true;
}

void setup() {
  // Manual begin() is required on core without built-in support e.g. mbed
  // rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
  // Register the CDC interface first (gets interfaces 0-1).
  MySerial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // Start I2S so it knows the audio format.
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.buffer_size = 256;
  cfg.buffer_count = 10;
  out.begin(cfg);

  // Capture the true capacity directly from the driver right after begin()
  // (buffer is empty here, so availableForWrite() == full capacity) --
  // avoids assuming buffer_size/buffer_count map 1:1 to availableForWrite()'s
  // units, which they don't on RP2040 (confirmed by the initial >100%
  // readings).
  i2s_buffer_capacity = (size_t)out.availableForWrite();

  // We can avoid a copy in the loop by triggering a write to i2s when we
  // receive a packet
  in.setRxDoneCallback(rxDone);

  // Register USB audio in RX mode (host → device, i.e. USB speaker).
  // begin_usb defaults to false so USB.begin() below controls the start.
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  config.fifo_packets = 1;
  in.begin(config);

  // If already enumerated, additional class driver begin() e.g msc, hid, midi
  // won't take effect until re-enumeration: on ESP32 you can alternatively call
  // USB.begin()
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  MySerial.println("USB audio RX + CDC started");
}

void loop() {}
