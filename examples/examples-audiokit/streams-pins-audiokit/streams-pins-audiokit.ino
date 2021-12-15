/**
 * @file stream-pins-audiokit.ino
 * @brief see
 * https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/stream-pins-audiokit/README.md
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "flite_arduino.h"

using namespace audio_tools;

AudioKitStream kit; 
Flite flite(kit);

void button1() { flite.say("Button One"); }
void button2() { flite.say("Button Two"); }
void button3() { flite.say("Button Three"); }
void button4() { flite.say("Button Four"); }

// Arduino setup
void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  //AUDIOKIT_LOG_LEVEL = AudioKitDebug; 

  auto cfg = kit.defaultConfig();
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  kit.begin(cfg);

  // Assign pins to methods
  kit.addAction(PIN_KEY1, button1);
  kit.addAction(PIN_KEY2, button2);
  kit.addAction(PIN_KEY3, button3);
  kit.addAction(PIN_KEY4, button4);

  // example with actions using lambda expression
  auto down = []() { AudioKitStream::actionVolumeDown(); flite.say("Volume down"); };
  kit.addAction(PIN_KEY5, down);
  auto up = []() { AudioKitStream::actionVolumeUp(); flite.say("Volume up");   };
  kit.addAction(PIN_KEY6, up);

  flite.say("Please push a button");
}

// Arduino Loop
void loop() { kit.processActions(); }
