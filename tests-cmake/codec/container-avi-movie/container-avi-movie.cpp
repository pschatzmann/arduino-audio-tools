/**
 * @file container-avi-movie.cpp
 * @author Phil Schatzmann
 * @brief Play AVI test movie downloaded from https://archive.org/embed/Test_Avi
 * To build execute the following steps
 * - mkdir build
 * - cd build
 * - cmake ..
 * - make
 * @version 0.1
 * @date 2022-04-30
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Disk/FileSystem.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"
#include "AudioTools/Video/OutputOpenCV.h"

PortAudioStream out;   // Output of sound on desktop
OutputOpenCV jpegDisplay;
DemuxerAVI codec;
DecoderHelix multiDecoder;  // WAV/AAC/MP3, auto-selected by mime (DecoderHelix
                            // bundles WAVDecoder + AACDecoderHelix + MP3DecoderHelix)
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> out
EncodedAudioOutput avi(&audioOut, &codec);
File file;
StreamCopy copier(avi, file);
VideoAudioBufferedSync videoSync(10*1024, -20);


void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  multiDecoder.begin();
  file.open("/data/resources/test1.avi",FILE_READ);
  codec.setOutputVideo(jpegDisplay);
  codec.setVideoAudioSync(&videoSync);
  //codec.setMute(true);
}

void loop() {
  if(!copier.copy()){
    stop();
  }
}
