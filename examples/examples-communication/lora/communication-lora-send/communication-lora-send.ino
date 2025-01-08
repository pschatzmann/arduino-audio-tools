/**
 * @file example-lora-send.ino
 * @author Phil Schatzmann
 * @brief Sending data over LoRa. We use a Heltec WiFi LoRa V3 board for testing.
 * V3 boards use the SX1262.
 * @version 0.1
 * @date 2023-11-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/Communication/LoRaStream.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
LoRaStream lora;
StreamCopy copier(lora, sound);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while(!Serial);
  Serial.println("starting...");

  // Setup LoRa 
  Serial.printf("SCK: %d, MISO: %d, MOSI: %d, SS=%d", SCK,MISO,MOSI,SS);
  SPI.begin(SCK, MISO, MOSI, SS);
  auto cfg = lora.defaultConfig();
  cfg.copyFrom(info);
  if (!lora.begin(cfg)){
    Serial.println("LoRa failed");
    stop();
  }

  // Setup sine wave
  sineWave.begin(info, N_B4);

  Serial.println("Sender started...");
}

void loop() { 
  copier.copy();
}