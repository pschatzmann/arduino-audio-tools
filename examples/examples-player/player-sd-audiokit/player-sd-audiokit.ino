/**
 * @file player-sd-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sd-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_SDFAT
#define USE_HELIX
#include "AudioTools.h"
#include "AudioDevices/ESP32AudioKit/AudioKit.h"

using namespace audio_tools;  

const char *startFilePath="/";
const char* ext="mp3";
int speedMz = 10;
AudioSourceSdFat source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS, speedMz);
AudioKitStream kit;
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);

void next() {
   player.next();
}

void previous() {
   player.previous();
}

void stopResume(){
  if (player.isActive()){
    player.stop();
  } else{
    player.play();
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

 // setup additional buttons 
  kit.addAction(PIN_KEY1, stopResume);
  kit.addAction(PIN_KEY4, next);
  kit.addAction(PIN_KEY3, previous);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  kit.begin(cfg);
  
  // setup player
  player.setVolume(0.7);
  player.begin();
}

void loop() {
  player.copy();
  kit.processActions();
}
