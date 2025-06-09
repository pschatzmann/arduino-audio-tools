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
#include "AudioTools/AudioCodecs/MP4Parser.h"
#include "SD.h"

MP4Parser parser;
File file;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  file = SD.open("/home/pschatzmann/Music/m4a/aac.m4a");
  if (!file.isOpen()) {
    Serial.println("Failed to open file!");
    return;
  }

  parser.begin();

  Serial.println("MP4 Boxes:");
}

void loop() {
  char buffer[1024];
  int to_read = min(sizeof(buffer), parser.availableForWrite());
  size_t bytesRead = file.readBytes(buffer,to_read);
  assert(parser.write(buffer, bytesRead)== bytesRead);
  if (bytesRead == 0) {
    Serial.println("End of file reached.");
    exit(0);  // Exit the process
  }
}