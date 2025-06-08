/**
 * @file test-codec-alac.ino
 * @author Phil Schatzmann
 * @brief generate sine wave -> encoder -> decoder -> audiokit (out)
 * @version 0.1
 * 
 * @copyright Copyright (c) 2025
 * 
 */
//#include "SD.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecALAC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioCodecs/ContainerM4A.h"
#include "AudioTools/AudioCodecs/MultiDecoder.h"
#include "AudioTools/AudioCodecs/M4AFileSampleSizeBuffer.h"
#include "AudioTools/Disk/AudioSourceSD.h"


const char *startFilePath="/home/pschatzmann/Music/m4a";
const char* ext="m4a";
AudioSourceSD source(startFilePath, ext);
CsvOutput<int16_t> out(Serial);
MultiDecoder multi_decoder;
ContainerM4A dec_m4a(multi_decoder);
AACDecoderHelix dec_aac;
MP3DecoderHelix dec_mp3;
DecoderALAC dec_alac;
AudioPlayer player(source, out, multi_decoder);
M4AFileSampleSizeBuffer sizes_buffer(player, dec_m4a);
//File buffer_file;
//RingBufferFile<File,stsz_sample_size_t> file_buffer(0);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup multi decoder
  multi_decoder.addDecoder(dec_m4a, "audio/m4a");
  multi_decoder.addDecoder(dec_alac,"audio/alac"); 
  multi_decoder.addDecoder(dec_aac,"audio/aac"); 
  multi_decoder.addDecoder(dec_mp3,"audio/mp3"); 

  // set custom buffer to optimize the memory usage
  // buffer_file = SD.open("/home/pschatzmann/tmp.tmp", O_RDWR | O_CREAT);
  // file_buffer.begin(buffer_file);
  // dec_m4a.setSampleSizesBuffer(file_buffer);
  dec_m4a.setSampleSizesBuffer(sizes_buffer);

  // setup output
  auto cfg = out.defaultConfig(TX_MODE);
  out.begin(cfg);

  //source.setTimeoutAutoNext(5000000);
  // setup player
  player.begin();
}

void loop() {
  player.copy();
}