/**
 * @file streams-talkie-audiokit.ino
 * We use the TalkiePCM TTS library to generate the audio
 * You need to install https://github.com/pschatzmann/TalkiePCM
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h" //https://github.com/pschatzmann/arduino-audio-driver
#include "TalkiePCM.h" // https://github.com/pschatzmann/TalkiePCM
#include "Vocab_US_Large.h"

const AudioInfo info(8000, 2, 16);
AudioBoardStream out(AudioKitEs8388V1);  // Audio sink
//CsvOutput<int16_t> out(Serial);
TalkiePCM voice(out, info.channels);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // setup AudioKit
  auto cfg = out.defaultConfig();
  cfg.copyFrom(info);
  out.begin(cfg);
  
  Serial.println("Talking...");
}

void loop() {
  voice.say(sp2_DANGER);
  voice.say(sp2_DANGER);
  voice.say(sp2_RED);
  voice.say(sp2_ALERT);
  voice.say(sp2_MOTOR);
  voice.say(sp2_IS);
  voice.say(sp2_ON);
  voice.say(sp2_FIRE);
  voice.silence(1000);
}