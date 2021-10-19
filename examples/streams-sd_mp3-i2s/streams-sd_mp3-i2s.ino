/**
 * @file streams-sd_mp3-i2s.ino
 * @author Phil Schatzmann
 * @brief decode MP3 file and output it on I2S
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021 
 */

// set this in AudioConfig.h or here after installing https://github.com/pschatzmann/arduino-libhelix.git
#define USE_HELIX 

#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

using namespace audio_tools;  

const int chipSelect=10;
I2SStream i2s; // final output of decoded stream
EncodedAudioStream decoder(i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier; 


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);  

  // setup file
  SD.begin(chipSelect);
  File audioFile = SD.open("/music.mp3");

  // setup i2s
  I2SConfig config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  decoder.setNotifyAudioChange(i2s);
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);

}

void loop(){
  if (!copier.copy()) {
    stop();
  }
}
