/**
 * @file player-sd.ino
 * @brief MP3 player for the Waveshare ESP32-P4-WIFI6: reads files off the
 * microSD card over SPI (SD.h) and plays them through the ES8311/speaker amp
 * path via I2SCodecStream and arduino-audio-driver's `WaveshareESP32P4WIFI6`
 * board, which also starts the SPI bus on the TF card pins. See
 * player-sdmmc.ino for the same player over SD_MMC.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * - https://github.com/pschatzmann/codec-helix
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <SD.h>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Disk/AudioSourceSD.h"

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceSD source(startFilePath, ext, PIN_WAVESHARE_ESP32P4WIFI6_SD_CS);
I2SCodecStream out(WaveshareESP32P4WIFI6);
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // also starts the SPI bus for the SD card
  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);
  out.setVolume(0.5);

  player.begin();
}

void loop() { player.copy(); }
