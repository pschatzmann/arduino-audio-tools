/**
 * @file streams-i2s-faust_copy-i2s.ino
 * @author Phil Schatzmann
 * @brief Example how to use Faust to write and process a stereo signal. The data
 * is just copied, keeping the left and right signal separate.
 * @version 0.1
 * @date 2022-04-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioLibs/AudioFaust.h"
#include "Copy.h"

AudioBoardStream io(AudioKitEs8388V1); 
FaustStream<mydsp> faust(io);  // final output to io
StreamCopy copier(faust, io);  // copy mic to faust

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