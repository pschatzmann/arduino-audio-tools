// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include <SPI.h>
#include <SD.h>

using namespace audio_tools;  

const int chipSelect=10;
AudioSourceCallback source(callbackInit, callbackStream);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);
File audioFile;


void callbackInit() {
  SD.begin(chipSelect);
  audioFile = SD.open("/");
}

void Stream* callbackStream() {
  audioFile.close();
  audioFile = audioFile.openNextFile();
  return &audioFile;
}

void callbackPrintMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(MetaDataTypeStr[type]);
  Serial.print(": ");
  Serial.println(str);
}


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  // setup output
  I2SConfig cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  player.setCallbackMetadata(callbackPrintMetaData);
  player.begin();
}

void loop() {
  player.copy();
}