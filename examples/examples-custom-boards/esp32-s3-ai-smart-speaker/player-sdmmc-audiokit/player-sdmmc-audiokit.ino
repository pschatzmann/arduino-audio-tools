/**
 * @file player-sdmmc-audiokit.ino
 * @brief SDMMC Audio Player example for ESP32-S3-AI-Smart-Speaker
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/arduino-libhelix
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h" // or AudioSourceIdxSDMMC.h
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "ESP32S3AISmartSpeaker.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDMMC source(startFilePath, ext);
AudioBoardStream kit(ESP32S3AISmartSpeaker);
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, kit, decoder);

void setup() {
  Serial.begin(115200);
  delay(2000);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;
  //cfg.sd_active = false;
  kit.begin(cfg);
  kit.setVolume(0.4);


  // setup player: must be after 
  player.begin();

  // select file with setPath() or setIndex()
  //player.setPath("/ZZ Top/Unknown Album/Lowrider.mp3");
  //player.setIndex(1); // 2nd file

}

void loop() {
  player.copy();
}