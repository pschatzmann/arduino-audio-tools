/**
 * @file streams-sd-audiokit.ino
 * @author Phil Schatzmann
 * @brief Just a small demo, how to use files with the SD library
 * @version 0.1
 * @date 2022-10-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


const int chipSelect=PIN_AUDIO_KIT_SD_CARD_CS;
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier; 
File audioFile;

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup audiokit before SD!
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = true;
  i2s.begin(config);

  // setup file
  SD.begin(chipSelect);
  audioFile = SD.open("/ZZ Top/Unknown Album/Lowrider.mp3");

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // begin copy
  copier.setCheckAvailableForWrite(false);
  copier.begin(decoder, audioFile);

}

void loop(){
  if (!copier.copy()) {
    stop();
  }
}