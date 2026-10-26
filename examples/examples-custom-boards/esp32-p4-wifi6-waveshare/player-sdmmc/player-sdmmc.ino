/**
 * @file player-sdmmc.ino
 * @brief MP3 player for the Waveshare ESP32-P4-WIFI6: reads files off the
 * microSD card (SD_MMC) and plays them through the ES8311/speaker amp path
 * via I2SCodecStream and arduino-audio-driver's `WaveshareESP32P4WIFI6`
 * board.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/codec-helix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <SD_MMC.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSDMMC source(startFilePath, ext);
I2SCodecStream out(WaveshareESP32P4WIFI6);
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  out.setVolume(0.5);

  // AudioSourceSDMMC::begin() calls SD_MMC.begin("/sdcard", true) - 1-bit
  // mode with the core's default SD pins/power, which match this board;
  // see sdmmc-test.ino.
  player.begin();
}

void loop() { player.copy(); }
