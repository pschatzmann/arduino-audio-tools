/**
 * @file player-sd-callback.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-player/player-callback-i2s/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


// Install https://github.com/pschatzmann/arduino-libhelix.git

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include <SPI.h>
#include <SD.h>


// forward declarations
void callbackInit();
Stream* callbackNextStream(int offset);

// data
const int chipSelect=PIN_CS;
AudioSourceCallback source(callbackNextStream, callbackInit);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);
File audioFile;
File dir;

void callbackInit() {
  // make sure that the directory contains only mp3 files
  dir = SD.open("/TomWaits");
}

Stream* callbackNextStream(int offset) {
  audioFile.close();
  // the next files must be a mp3 file
  for (int j=0;j<offset;j++)
    audioFile = dir.openNextFile();
  return &audioFile;
}

void callbackPrintMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}


void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup SD
  SD.begin(chipSelect);

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  i2s.begin(cfg);

  // setup player
  player.setMetadataCallback(callbackPrintMetaData);
  player.begin();
}

void loop() {
  player.copy();
}