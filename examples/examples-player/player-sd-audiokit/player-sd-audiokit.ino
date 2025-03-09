/**
 * @file player-sd-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/player-sd-audiokit/README.md
 * Make sure that the pins are set to off, on, on, off, off
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceSD.h" // or AudioSourceIdxSD.h
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS);
AudioBoardStream kit(AudioKitEs8388V1);
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, kit, decoder);

void next(bool, int, void*) {
   player.next();
}

void previous(bool, int, void*) {
   player.previous();
}

void startStop(bool, int, void*) {
   player.setActive(!player.isActive());
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  // sd_active is setting up SPI with the right SD pins by calling 
  // SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);
  cfg.sd_active = true;
  kit.begin(cfg);

  // setup additional buttons 
  kit.addDefaultActions();
  kit.addAction(kit.getKey(1), startStop);
  kit.addAction(kit.getKey(4), next);
  kit.addAction(kit.getKey(3), previous);


  // setup player
  player.setVolume(0.7);
  player.begin();

  // select file with setPath() or setIndex()
  //player.setPath("/ZZ Top/Unknown Album/Lowrider.mp3");
  //player.setIndex(1); // 2nd file

}

void loop() {
  player.copy();
  kit.processActions();

}