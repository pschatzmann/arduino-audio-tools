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
#include "AudioTools/AudioLibs/A2DPStream.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"


A2DPStream in;
AudioBoardStream kit(AudioKitEs8388V1);
StreamCopy copier(kit, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

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