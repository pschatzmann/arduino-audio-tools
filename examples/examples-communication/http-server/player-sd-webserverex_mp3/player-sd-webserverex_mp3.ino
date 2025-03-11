/**
 * @file player-sdfat-a2dp.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sdfat-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioLibs/AudioServerEx.h"
#include "AudioTools/AudioCodecs/CodecCopy.h"

#define PIN_AUDIO_KIT_SD_CARD_CS 13
#define PIN_AUDIO_KIT_SD_CARD_MISO 2
#define PIN_AUDIO_KIT_SD_CARD_MOSI 15
#define PIN_AUDIO_KIT_SD_CARD_CLK  14

const char *ssid = "SSID";
const char *password = "PWD";
const char *startFilePath="/";
const char* ext="mp3";

AudioSourceSD source(startFilePath, ext, PIN_AUDIO_KIT_SD_CARD_CS);
AudioServerEx out;
AudioPlayer player(source, out, *new CopyDecoder());

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  HttpLogger.setLevel(tinyhttp::Warning);

  // setup SPI for SD card
  SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO, PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);


  // setup output - We need to login and serve the data as audio/mp3
  auto cfg = out.defaultConfig();
  cfg.password = password;
  cfg.ssid = ssid;
  cfg.mime = "audio/mp3";
  out.begin(cfg);

  // setup player
  player.setVolume(1.0);
  player.begin();

}

void loop() {
  player.copy();
  out.copy();
}