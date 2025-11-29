/**
 * @file player-sdfat-audiokit.ino
 * @brief SDFAT Audio Player example for ESP32-S3-AI-Smart-Speaker.
 * Unfortunatly the cd pins is mapped to EXIO4 and not an regualr GPIO pin.
 * This requires custom functions for SdCsInit and SdCsWrite. Make sure that
 * SD_CHIP_SELECT_MODE is set to 1 in SdFatConfig.h.
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
AudioSourceSDFAT source(startFilePath, ext);
AudioBoardStream kit(ESP32S3AISmartSpeaker);
MP3DecoderHelix decoder;  // or change to MP3DecoderMAD
AudioPlayer player(source, kit, decoder);

// do nothing: pins are setup by the baord driver
void sdCsInit(SdCsPin_t pin) {}

// activate/deactivate the CS pin
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