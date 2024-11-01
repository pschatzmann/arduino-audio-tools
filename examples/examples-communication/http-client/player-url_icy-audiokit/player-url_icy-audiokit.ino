/**
 * @file player-url-kit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/player-url-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

const char *urls[] = {
  "http://stream.srg-ssr.ch/m/rsj/mp3_128",
  "http://stream.srg-ssr.ch/m/drs3/mp3_128",
  "http://stream.srg-ssr.ch/m/rr/mp3_128",
  "http://sunshineradio.ice.infomaniak.ch/sunshineradio-128.mp3",
  "http://streaming.swisstxt.ch/m/drsvirus/mp3_128"
};
const char *wifi = "wifi";
const char *password = "password";

ICYStream urlStream(wifi, password);
AudioSourceURL source(urlStream, urls, "audio/mp3");
AudioBoardStream kit(AudioKitEs8388V1);
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);

void next(bool, int, void*) {
   player.next();
}

void previous(bool, int, void*) {
   player.previous();
}

void stopResume(bool, int, void*){
  if (player.isActive()){
    player.stop();
  } else{
    player.play();
  }
}

// Arduino setup
void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);

  // setup navigation
  kit.addAction(kit.getKey(4), next);
  kit.addAction(kit.getKey(3), previous);

  // setup player
  player.setVolume(0.7);
  player.begin();
}

// Arduino loop
void loop() {
  player.copy();
  kit.processActions();
}