/**
 * @file streams-a2dp-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-a2dp-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"
#include "AudioLibs/AudioKit.h"


A2DPStream in;
AudioKitStream kit;
StreamCopy copier(kit, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start the bluetooth audio receiver
  Serial.println("starting A2DP...");
  auto cfg = in.defaultConfig(RX_MODE);
  cfg.name = "AudioKit";
  in.begin(cfg);  

  // setup the audioKit
  auto cfgk = kit.defaultConfig(TX_MODE);
  cfgk.copyFrom(in.audioInfo());
  kit.begin(cfgk);

}

// Arduino loop  
void loop() {
  copier.copy();
}