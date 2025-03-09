/**
 * @file player-sdfat-analog.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-sdfat-analog/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT source(startFilePath, ext);
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
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

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
