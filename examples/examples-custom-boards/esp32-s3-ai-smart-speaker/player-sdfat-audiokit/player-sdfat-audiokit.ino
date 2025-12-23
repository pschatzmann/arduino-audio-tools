/**
 * @file player-sdfat-audiokit.ino
 * @brief SDFAT Audio Player example for ESP32-S3-AI-Smart-Speaker.
 * Unfortunatly the SD cd pin is mapped to EXIO4 and not to a regular GPIO pin.
 * This requires the custom functions SdCsInit and SdCsWrite. Make sure that
 * SD_CHIP_SELECT_MODE is set to 1 in SdFatConfig.h.
 * 
 * This makes sure that the SD is only active when it is actually used, so that
 * we can extend the battery life!
 * 
 * Please note that this examples also works w/o the custom functions since the 
 * AudioKit driver already configures the EXIO4 pin for SD use.
 * 
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/greiman/SdFat
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"  // or AudioSourceIdxSDMMC.h
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "ESP32S3AISmartSpeaker.h"

const char* startFilePath = "/";
const char* ext = "mp3";
AudioSourceSDFAT source(startFilePath, ext, -1);
AudioBoardStream kit(ESP32S3AISmartSpeaker);
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, kit, decoder);

// SDFAT: do nothing: pins are setup by the baord driver
void sdCsInit(SdCsPin_t pin) {}

// SDFAT: activate/deactivate the CS pin
void sdCsWrite(SdCsPin_t pin, bool level) {
  kit.digitalWrite(EXIO4, level);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  //cfg.sdmmc_active = false;
  cfg.sd_active = true;
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