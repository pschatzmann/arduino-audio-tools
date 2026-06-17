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
 *   → tx_queue_ → process() → USB isochronous IN endpoint → host
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
StreamCopy copier(out, sound);                             

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start USB
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
  copier.copy();
  out.process();
}
