/**
 * @file test-codec-alac.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/M4AAudioFileDemuxer.h"
#include "SD.h"

//MP4Parser parser;
M4AAudioFileDemuxer demux;
File file;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  file = SD.open("/home/pschatzmann/Music/m4a/1-07 All You Need Is Love.m4a");
  if (!file.isOpen()) {
    Serial.println("Failed to open file!");
    return;
  }

  if (!demux.begin(file)){
    Serial.println("Failed to open demuxer!");
    return;
  }


  Serial.println("MP4 Boxes:");
}

void loop() {
  demux.copy();
}