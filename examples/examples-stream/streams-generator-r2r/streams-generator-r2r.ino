/**
 * @file streams-generator-r2r.ino
 * @author Phil Schatzmann
 * @brief Example for a self built resistor ladder DAC 
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/AudioLibs/R2ROutput.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave;                       // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
R2ROutput out; 
StreamCopy copier(out, sound);                             // copies sound into i2s
const int pins1[] = {12,14,27,26,25,33,32, 35};            // ESP32 pins

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // start I2S
  Serial.println("starting R2R...");
  auto config = out.defaultConfig();
  config.copyFrom(info); 
  // 8 pins for 8 bit DAC for channel 1
  config.channel1_pins = pins1;
  // channel 2 would be config.channel2_pins
  out.begin(config);

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
