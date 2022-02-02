/**
 * @file player-sd-i2s.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sd-i2s/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 
#define USE_SDFAT

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"



const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSdFat source(startFilePath, ext);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);


void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.setMetadataCallback(printMetaData);
  player.begin();
}

void loop() {
  player.copy();
}