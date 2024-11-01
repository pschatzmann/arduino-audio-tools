/**
 * @file streams-sd_mp3-i2s.ino
 * @author Phil Schatzmann
 * @brief decode MP3 file and output it on I2S
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021 
 */


#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


const int chipSelect=10;
I2SStream i2s; // final output of decoded stream
EncodedAudioStream decoder(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier; 
File audioFile;

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup file
  SD.begin(chipSelect);
  audioFile = SD.open("/Music/Conquistadores.mp3");

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  decoder.begin();

  // begin copy
  copier.begin(decoder, audioFile);

}

void loop(){
  if (!copier.copy()) {
    stop();
  }
}
