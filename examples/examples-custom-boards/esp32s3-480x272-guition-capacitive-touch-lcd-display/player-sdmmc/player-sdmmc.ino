/**
 * @file player-sdmmc.ino
 * @brief MP3 player for the Guition ESP32-S3 4.3" 480x272 display: reads
 * files off the microSD card (SD_MMC, 1-bit mode) and plays them through
 * the onboard NS4168 I2S speaker amp.
 *
 * Ported from the esp32-s3-240x320-lcd-display example's player-sdmmc.ino,
 * adapted because this board has no audio codec (unlike that board's
 * ES8311-based AudioBoardStream): output goes through plain I2SStream
 * instead, and SD_MMC.setPins() is called explicitly before player.begin()
 * since AudioSourceSDMMC's begin() only calls SD_MMC.begin() (with
 * whatever pins were set previously), not setPins() itself.
 *
 * All pins below are confirmed working on real hardware (2026-08-19).
 * See sdmmc-test.ino re: the SD card's DAT3 line (what the user called
 * "CS", GPIO10) - not one of the pins ESP32-S3's SD_MMC.setPins(clk,
 * cmd, d0) 1-bit overload manages, and confirmed to make no difference
 * whether driven or left alone, so it's not used here either.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-libhelix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <SD_MMC.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"

// SD pins, confirmed working - see sdmmc-test.ino.
constexpr int kPinSdClk = 12;
constexpr int kPinSdCmd = 11;
constexpr int kPinSdD0 = 13;
// I2S pins, confirmed working - see audio-out.ino.
constexpr int kPinBck = 42;
constexpr int kPinWs = 2;
constexpr int kPinData = 41;

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSDMMC source(startFilePath, ext);
I2SStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD_MMC.setPins(kPinSdClk, kPinSdCmd, kPinSdD0)) {
    Serial.println("SD_MMC.setPins() failed");
    stop();
  }

  auto cfg = out.defaultConfig(TX_MODE);
  cfg.pin_bck = kPinBck;
  cfg.pin_ws = kPinWs;
  cfg.pin_data = kPinData;
  out.begin(cfg);

  player.begin();  // AudioSourceSDMMC::begin() calls SD_MMC.begin("/sdcard", true)
}

void loop() { player.copy(); }
