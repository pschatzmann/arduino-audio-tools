/**
 * @file streams-sd-audiokit.ino
 * @author Phil Schatzmann
 * @brief Just a small demo, how to use files with the SD library with a streaming decoder
 * @version 0.1
 * @date 2022-10-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioCodecs/CodecFLAC.h"


const int chipSelect=PIN_AUDIO_KIT_SD_CARD_CS;
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
FLACDecoder dec;
File audioFile;

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup audiokit before SD!
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = true;
  i2s.begin(config);

  // setup file
  SD.begin(chipSelect);
  audioFile = SD.open("/flac/test2.flac");

  // setup decoder
  dec.setInput(audioFile);
  dec.setOutput(i2s);
  dec.begin();

}

void loop(){
  dec.copy();
}