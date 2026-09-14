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
#include "AudioTools/Communication/AudioHttp.h"

URLStream url("ssid","password"); // input
AudioBoardStream out(AudioKitEs8388V1);
DecoderL8 l8(false);
EncodedAudioStream audioOut(&out, &l8);  // decodes L8 -> 16 bit -> out
DemuxerAVI codec;
EncodedAudioOutput avi(&codec);  // bridges raw AVI bytes -> codec.write()
StreamCopy copier(avi, url);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output using default settings
  out.begin(out.defaultConfig());
  codec.setOutputAudio(audioOut);

  // open url
  url.begin("https://archive.org/download/Test_Avi/MVI_0043.AVI");

  // parse metadata, so that we can validate if the audio/video is supported
  while(!codec.isMetadataReady()){
    copier.copy();
  }
}

void loop() {
  // Process data if audio is PCM
  AudioInfoFormat info = codec.getAudioInfo();
  if (info.format==AudioFormat::PCM && info.bits_per_sample==8){
      copier.copy();
  }
}
