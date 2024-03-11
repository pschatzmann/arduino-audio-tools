/**
 * @file communication-container-binary.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/ContainerMP4.h"
//#include "AudioLibs/StdioStream.h"
//const char *file = "/home/pschatzmann/Development/Mp4Parser/sample-1.m4a";
const char* file_str = "/home/pschatzmann/Downloads/test.m4a";
auto file = SD.open(file_str, FILE_READ);
CsvOutput<int16_t> out(Serial);
ContainerMP4 mp4;
EncodedAudioStream codec(&out, &mp4);
StreamCopy copier(codec, file);     

void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start 
  Serial.println("starting...");
  auto cfgi = out.defaultConfig(TX_MODE);
  out.begin(cfgi);

  codec.begin();

  Serial.println("Test started...");
}


void loop() { 
  if (!copier.copy()){
    exit(0);
  }
}