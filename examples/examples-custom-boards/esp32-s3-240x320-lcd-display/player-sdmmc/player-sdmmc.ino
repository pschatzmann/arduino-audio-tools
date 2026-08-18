/**
 * @file player-sdmmc.ino
 * @brief MP3 player for the Hosyond 2.8" ESP32-S3 Display: reads files off
 * the microSD card (4-bit SDIO) and plays them through the ES8311/FM8002E
 * speaker path.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/arduino-libhelix
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSDMMC source(startFilePath, ext);
AudioBoardStream kit(ESP32S3HosyondDisplay);
MP3DecoderHelix decoder;
AudioPlayer player(source, kit, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sdmmc_active = true;  // board's begin() calls SD_MMC.setPins()+begin()
  kit.begin(cfg);
  kit.setVolume(0.5);

  player.begin();  // must be after kit.begin()
}

void loop() { player.copy(); }
