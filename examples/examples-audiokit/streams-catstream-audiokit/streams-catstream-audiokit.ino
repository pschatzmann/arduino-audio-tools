/**
 * @file streams-catstream-audiokit.ino
 * @author Phil Schatzmann
 * @brief Demo how to use a CatStream to concatenate multiple audio files and use it with the AudioKitEs8388V1
 * @version 0.1
 * @date 2022-10-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "FS.h"
#include "SD_MMC.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

AudioBoardStream i2s(AudioKitEs8388V1);
MP3DecoderHelix mp3;
File audioFile;
EncodedAudioStream encoded(&audioFile, &mp3); // Decoding stream
NullStream nullStream;
CatStream cat(true);
StreamCopy copier(i2s, cat); 


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  
  delay(5000);
  Serial.println("starting");

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
  audioFile = SD_MMC.open("/Music/mp3-test/44100.mp3");
  if (!audioFile){
    Serial.println("File does not exist");
    stop();
  }

  // mp3 file followed by silence
  cat.add(encoded);
  cat.add(nullStream);
  cat.begin();

  // start decoder stream
  encoded.begin();
}

void loop(){
  copier.copy();
}