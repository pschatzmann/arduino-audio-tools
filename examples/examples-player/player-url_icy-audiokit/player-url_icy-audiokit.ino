/**
 * @file player-url-kit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-url-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioDevices/ESP32AudioKit/AudioKit.h"

using namespace audio_tools;  

const char *urls[] = {
  "http://centralcharts.ice.infomaniak.ch/centralcharts-128.mp3",
  "http://centraljazz.ice.infomaniak.ch/centraljazz-128.mp3",
  "http://centralrock.ice.infomaniak.ch/centralrock-128.mp3",
  "http://centralcountry.ice.infomaniak.ch/centralcountry-128.mp3",
  "http://centralfunk.ice.infomaniak.ch/centralfunk-128.mp3"
};
const char *wifi = "wifi";
const char *password = "password";

ICYStream urlStream(wifi, password);
AudioSourceURL source(urlStream, urls, "audio/mp3");
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

// Arduino setup
void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup navigation
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

// Arduino loop
void loop() {
  player.copy();
  kit.processActions();
}