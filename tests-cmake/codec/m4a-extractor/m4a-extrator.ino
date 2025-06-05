/**
 * @file m4a-extractor.ino
 * @author Phil Schatzmann
 * @brief Decode M4A file and output to CSV
 * @version 0.1
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/M4AAudioFileDemuxer.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "SD.h"

MultiDecoder multi_decoder;
AACDecoderHelix dec_aac;
DecoderALAC dec_alac;
CsvOutput<int16_t> out(Serial);

//MP4Parser parser;
M4AAudioFileDemuxer demux;
File file;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
    return;
  }

  file = SD.open("/home/pschatzmann/Music/m4a/1-07 All You Need Is Love.m4a");
  if (!file) {
    Serial.println("Failed to open file!");
    return;
  }

  // Setup decoder
  multi_decoder.setOutput(out);
  multi_decoder.addDecoder(dec_aac, "audio/aac");
  multi_decoder.addDecoder(dec_alac, "audio/alac"); 
  demux.setDecoder(multi_decoder);

  if (!demux.begin(file)){
    Serial.println("Failed to open demuxer!");
    return;
  }
}

void loop() {
  demux.copy();
}