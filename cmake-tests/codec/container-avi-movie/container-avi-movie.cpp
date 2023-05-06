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
#include "AudioCodecs/ContainerAVI.h"
#include "AudioLibs/Desktop/File.h"
#include "AudioLibs/PortAudioStream.h"
#include "Video/JpegOpenCV.h"

PortAudioStream out;   // Output of sound on desktop 
JpegOpenCV jpegDisplay;
AVIDecoder codec(new DecoderL8(), &jpegDisplay);
EncodedAudioOutput avi(&out, &codec);
File file;
StreamCopy copier(avi, file);
VideoAudioBufferedSync videoSync(10*1024, -20);


void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  file.open("/data/resources/test1.avi",FILE_READ);
  codec.setOutputVideoStream(jpegDisplay);
  codec.setVideoAudioSync(&videoSync);
  //codec.setMute(true);
}

void loop() {
  if(!copier.copy()){
    stop();
  }
}
