/**
 * @file streams-file_loop-audiokit.ino
 * @author Phil Schatzmann
 * @brief Just a small demo, how to use looping (=repeating) files with the SD library
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
#include "AudioLibs/FileLoop.h"
#include "AudioCodecs/CodecMP3Helix.h"


const int chipSelect=PIN_AUDIO_KIT_SD_CARD_CS;
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
FileLoop loopingFile;
StreamCopy copier(decoder, loopingFile); 

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup audiokit before SD!
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = true;
  i2s.begin(config);

  // setup file
  SD.begin(chipSelect);
  loopingFile.setFile(SD.open("/ZZ Top/Unknown Album/Lowrider.mp3"));
  //loopingFile.setLoopCount(-1); // define loop count
  //audioFile.setStartPos(44); // restart from pos 44
  //if ((audioFile.size()-44) % 1024!=0) audioFile.setSize((((audioFile.size()-44)/1024)+1)*1024+44);
  loopingFile.begin();

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();
}

void loop(){
  copier.copy();
}