/**
 * @file streams-faust-i2s.ino
 * @author Phil Schatzmann
 * @brief Example how to use Faust
 * @version 0.1
 * @date 2022-04-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioFaust.h"
#include "Noise.h"

AudioKitStream out; 
mydsp dsp;
FaustStream faust(dsp);
StreamCopy copier(out, faust);  // copy mic to tfl

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup sine wave
  auto cfg = faust.defaultConfig();
  faust.begin(cfg);

  // start I2S
  auto config = out.defaultConfig(TX_MODE);
  config.sample_rate = cfg.sample_rate; 
  config.channels = cfg.channels;
  config.bits_per_sample = cfg.bits_per_sample;
  out.begin(config);

}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}