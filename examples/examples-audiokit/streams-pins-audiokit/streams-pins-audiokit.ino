/**
 * @file stream-pins-audiokit.ino
 * @brief see
 * https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/streams-pins-audiokit/README.md
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "flite_arduino.h"

AudioBoardStream kit(AudioKitEs8388V1); 
Flite flite(kit);

void button1(bool, int, void*) { flite.say("Button One"); }
void button2(bool, int, void*) { flite.say("Button Two"); }
void button3(bool, int, void*) { flite.say("Button Three"); }
void button4(bool, int, void*) { flite.say("Button Four"); }

// Arduino setup
void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  //AUDIOKIT_LOG_LEVEL = AudioKitDebug; 

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  cfg.sd_active = false;
  kit.begin(cfg);

  // Assign pins to methods
  kit.addAction(kit.getKey(1), button1);
  kit.addAction(kit.getKey(2), button2);
  kit.addAction(kit.getKey(3), button3);
  kit.addAction(kit.getKey(4), button4);

  // example with actions using lambda expression
  auto down = [](bool,int,void*) { AudioBoardStream::actionVolumeDown(true, -1, nullptr); flite.say("Volume down"); };
  kit.addAction(kit.getKey(5), down);
  auto up = [](bool,int,void*) { AudioBoardStream::actionVolumeUp(true, -1, nullptr ); flite.say("Volume up");   };
  kit.addAction(kit.getKey(6), up);

  flite.say("Please push a button");
}

// Arduino Loop
void loop() { kit.processActions(); }
