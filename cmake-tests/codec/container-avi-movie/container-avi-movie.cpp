/**
 * @file communication-container-binary.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * @date 2022-04-30
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "AudioCodecs/ContainerAVI.h"
#include "AudioLibs/Desktop/File.h"
#include "AudioTools.h"

MiniAudioStream out;                   // output
DecoderL8 l8;                          // audio decoder
AVIDecoder codec(&l8);                 // container
EncodedAudioOutput avi(&out, &codec);  // avi stream
File file;                             // data input
StreamCopy copier(avi, file);
JpegDisplay jpeg;                      // display jpeg movie

void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  file.open("/data/resources/test1.avi", FILE_READ);
  // jpec decoder
  codec.setOutputVideoStream(jpeg);
}

void loop() {
  if (!copier.copy()) {
    stop();
  }
}
