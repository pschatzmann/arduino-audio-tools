/**
 * @file player-spiffs-i2s.ino
 * @brief example using the SPIFFS library
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSPIFFS.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSPIFFS source(startFilePath, ext);
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
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

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