/**
 * @file streams-faust_noise-i2s.ino
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
#include "Copy.h"

AudioKitStream io; 
FaustStream<mydsp> faust(io);
StreamCopy copier(faust, io);  // copy mic to tfl

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup Faust
  auto cfg = faust.defaultConfig();
  faust.begin(cfg);


  // start I2S
  auto cfg_i2s = io.defaultConfig(RXTX_MODE);
  cfg_i2s.copyFrom(cfg);
  io.begin(cfg_i2s);

}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}