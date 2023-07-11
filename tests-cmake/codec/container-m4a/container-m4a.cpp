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
#include "AudioCodecs/ContainerMPEG4.h"
//#include "AudioLibs/StdioStream.h"

auto file = SD.open("/home/pschatzmann/Development/Mp4Parser/sample-1.m4a", FILE_READ);
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