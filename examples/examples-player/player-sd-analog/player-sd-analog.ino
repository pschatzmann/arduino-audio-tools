/**
 * @file player-sd-analog.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sd-analog/README.md
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
AnalogAudioStream out;
MP3DecoderHelix decoder;
AudioPlayer player(source, out, decoder);


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
  auto cfg = out.defaultConfig();
  out.begin(cfg);

  // setup player
  //source.setFileFilter("*Bob Dylan*");
  player.setMetadataCallback(printMetaData);
  player.begin();
}

void loop() {
  player.copy();
}
