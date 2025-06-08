/**
 * @file test-codec-alac.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (i2s)
 * @version 0.1
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/ContainerM4A.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/AudioCodecs/M4AFileSampleSizeBuffer.h"
#include "SD.h"

MultiDecoder multi_decoder;
ContainerM4A dec_m4a(multi_decoder);
AACDecoderHelix dec_aac;
DecoderALAC dec_alac;
CsvOutput<int16_t> out(Serial);
EncodedAudioOutput decoder_output(&out, &dec_m4a); 
File file;
StreamCopy copier(decoder_output, file);     

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()){
    Serial.println("SD Card initialization failed!");
    return;
  }

  file = SD.open("/home/pschatzmann/Music/m4a/1-07 All You Need Is Love.m4a");
  if (!file) {
    Serial.println("Failed to open file!");
    return;
  }

  // mp4 supports alac and aac
  multi_decoder.addDecoder(dec_alac,"audio/alac"); 
  multi_decoder.addDecoder(dec_aac,"audio/aac"); 

  // start decoder output
  if(!decoder_output.begin()) {
    Serial.println("Failed to start decoder output!");
    return;
  }

  // start csv output
  if (!out.begin()){
    Serial.println("Failed to start CSV output!");
    return;
  }

  Serial.println("M4A decoding started...");
}


void loop() { 
  copier.copy();
}