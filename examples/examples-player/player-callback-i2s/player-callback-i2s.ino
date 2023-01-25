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
#include "AudioCodecs/CodecMP3Helix.h"
#include <SPI.h>
#include <SD.h>


// forward declarations
void callbackInit();
Stream* callbackStream(int offset);


// data
const int chipSelect=PIN_CS;
AudioSourceCallback source(callbackStream,callbackInit);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);
File audioFile;


void callbackInit() {
  SD.begin(chipSelect);
  audioFile = SD.open("/");
}

Stream* callbackStream(int offset) {
  auto lastFile = audioFile;
  audioFile = audioFile.openNextFile();
  lastFile.close();
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
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

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