/**
 * @file streams-sdmmc_wav-audiokit.ino
 * @author Phil Schatzmann
 * @brief A simple example that shows how to play (eg. a 24bit) wav file  
 * @date 2021-11-07
 * 
 * @copyright Copyright (c) 2021
 */


#include "FS.h"
#include "SD_MMC.h"
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream i2s;
WAVDecoder wav;
EncodedAudioStream encoded(&i2s, &wav); // Decoding stream
File audioFile;
StreamCopy copier(encoded, audioFile); 

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  // setup audiokit before SD!
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = false;
  i2s.begin(config);
  i2s.setVolume(1.0);

  // open sdmmc
  if (!SD_MMC.begin("/sdcard", false)){
    Serial.println("SD_MMC Error");
    stop();
  }
  // open file
  audioFile = SD_MMC.open("/wav24/test.wav");
  if (!audioFile){
    Serial.println("File does not exist");
    stop();
  }

  // start decoder stream
  encoded.begin();
}

void loop(){
  copier.copy();
}