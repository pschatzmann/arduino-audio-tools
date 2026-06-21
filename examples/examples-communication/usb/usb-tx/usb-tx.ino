/**
 * @file usb-tx.ino (microphone)
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
 *   → ep_in_ff → audiod_xfer_cb → USB isochronous IN endpoint → host
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
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
USBAudioStream out;
StreamCopy copier(out, sound, 80);

void setup(void) {
  // Manual begin() is required on core without built-in support e.g. mbed rp2040
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  // On ESP32-S3 with "USB CDC On Boot: Disabled", Serial = Serial0 (UART).
  MySerial.begin(115200);
  AudioToolsLogger.begin(MySerial, AudioToolsLogLevel::Warning);

  // Start generating the sine wave at 440 Hz (A4) with the default audio config.
  sineWave.begin(info, N_B4);

  // USB audio must be started before any other USB device (e.g. USB CDC).
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // If already enumerated, additional class driver begin() e.g msc, hid, midi
  // won't take effect until re-enumeration: on ESP32 you can alternatively call USB.begin()
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  MySerial.println("USB audio started");

}

void loop() {
  if (out.isStreamingActiveTx()) copier.copy();
  static unsigned long last_ms = 0;
  if (millis() - last_ms > 1000) {
    last_ms = millis();
    Serial.print("xfer="); Serial.print(out.getTxXferCount());
    Serial.print(" rd="); Serial.print(out.getTxFifoReadTotal());
    Serial.print(" avail="); Serial.print(out.availableForWrite());
    Serial.print(" filled="); Serial.print(out.bufferTx().bufferCountFilled());
    Serial.print(" empty="); Serial.print(out.bufferTx().bufferCountEmpty());
    Serial.print(" frame="); Serial.print(out.getTxFrameBytesLast());
    Serial.print(" bps="); Serial.println(out.getTxBytesPerSample());
  }

}
