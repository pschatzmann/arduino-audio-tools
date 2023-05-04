/**
 * @file test-container-avi.ino
 * @author Phil Schatzmann
 * @brief Test avi container with pcm data
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/ContainerAVI.h"
#include "AudioLibs/AudioKit.h"

URLStream url("ssid","password"); // input
AudioKitStream out;               // output
AVIDecoder codec;
EncodedAudioStream avi(&out, &codec);
StreamCopy copier(avi, url);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  url.begin("https://archive.org/download/Test_Avi/MVI_0043.AVI");

  // parse metadata, so that we can validate if the audio/video is supported
  while(!codec.isMetadataReady()){
    copier.copy();
  }
}

void loop() {
  // Process data if audio is PCM  
  if (codec.audioFormat()==1){
      copier.copy();
  }  
}
