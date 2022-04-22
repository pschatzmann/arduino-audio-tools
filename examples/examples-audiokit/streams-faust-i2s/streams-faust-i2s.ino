/**
 * @file streams-faust-i2s.ino
 * @author Phil Schatzmann
 * @brief Example how to use Faust as Audio Source
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

mydsp dsp;
FaustStream faust(dsp);
AudioKitStream out; 
StreamCopy copier(out, faust);  // copy mic to tfl

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup Faust
  auto cfg = faust.defaultConfig();
  faust.begin(cfg);

  // start I2S
  auto cfg_i2s = out.defaultConfig(TX_MODE);
  cfg_i2s.sample_rate = cfg.sample_rate; 
  cfg_i2s.channels = cfg.channels;
  cfg_i2s.bits_per_sample = cfg.bits_per_sample;
  out.begin(cfg_i2s);

}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}