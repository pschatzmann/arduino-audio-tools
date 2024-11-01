/**
 * @file test-container-avi.ino
 * @author Phil Schatzmann
 * @brief Test avi container with pcm 8bit data
 * @version 0.1
 * @date 2022-04-30
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

URLStream url("ssid","password"); // input
AudioBoardStream out(AudioKitEs8388V1); 
DecoderL8 l8(false);             
AVIDecoder codec(&l8);
EncodedAudioStream avi(&out, &codec);
StreamCopy copier(avi, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
 
  // setup output using default settings
  out.begin(out.defaultConfig());

  // open url
  url.begin("https://archive.org/download/Test_Avi/MVI_0043.AVI");

  // parse metadata, so that we can validate if the audio/video is supported
  while(!codec.isMetadataReady()){
    copier.copy();
  }
}

void loop() {
  // Process data if audio is PCM  
  if (codec.audioFormat()==AudioFormat::PCM && codec.audioInfo().bits_per_sample==8){
      copier.copy();
  }  
}
