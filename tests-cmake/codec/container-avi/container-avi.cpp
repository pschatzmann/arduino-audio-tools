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
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/ContainerAVI.h"
#include "AudioTools/AudioCodecs/CodecHelix.h"
#include "AudioTools/Disk/FileSystem.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"

//CsvOutput<int16_t> out;
PortAudioStream out;   // Output of sound on desktop
DemuxerAVI codec;
DecoderHelix multiDecoder;  // WAV/AAC/MP3, auto-selected by mime (DecoderHelix
                            // bundles WAVDecoder + AACDecoderHelix + MP3DecoderHelix)
EncodedAudioStream audioOut(&out, &multiDecoder);  // decodes PCM/AAC/MP3 -> out
EncodedAudioOutput riff(&audioOut, &codec);
File file;
StreamCopy copier(riff, file);

void setup() {
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  multiDecoder.begin();
  file.open("/data/resources/test1.avi",FILE_READ);
}

void loop() {
  if(!copier.copy()){
    stop();
  }
}
