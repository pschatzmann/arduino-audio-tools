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
#include "Experiments/ContainerAVI.h"
#include "AudioLibs/Desktop/File.h"

CsvStream<int16_t> csv;
ContainerAVI codec;
EncodedAudioStream riff(csv, codec);
File file;
StreamCopy copier(riff, file);

void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  file.open("/data/resources/test.avi",FILE_READ);
}

void loop() {
  copier.copy();
}
