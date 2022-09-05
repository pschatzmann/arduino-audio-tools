/**
 * @file player-sdfat-a2dp.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sdfat-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioLibs/AudioSourceSD.h"
#include "AudioLibs/AudioServerEx.h"
#include "AudioCodecs/CodecCopy.h"

const char *ssid = "SSID";
const char *password = "PWD";
const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext);
AudioServerEx out;
AudioPlayer player(source, out, *new CopyDecoder());

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

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