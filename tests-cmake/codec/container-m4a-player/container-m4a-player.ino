/**
 * @file container-m4a-player.ino
 * @author Phil Schatzmann
 * @brief Player with M4A files using a single MultiDecoder. However
 * I recommend to use 2 separate multi decoders one for the player and 
 * a separate one for the M4A container.
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
#include "WiFi.h"
#include "AudioTools/Communication/RedisBuffer.h"

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
// Option 1 - using the played file
//M4AFileSampleSizeBuffer sizes_buffer(player, dec_m4a);
// Option 2 - using a file to buffer
//File buffer_file;
//RingBufferFile<File,stsz_sample_size_t> file_buffer(0);
// Option 3 - using redis 
//WiFiClient client;
//RedisBuffer<stsz_sample_size_t> redis(client,"m4a-buffer1",0, 1024, 0);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup multi decoder
  multi_decoder.addDecoder(dec_m4a, "audio/m4a");
  multi_decoder.addDecoder(dec_alac,"audio/alac"); 
  multi_decoder.addDecoder(dec_aac,"audio/aac"); 
  multi_decoder.addDecoder(dec_mp3,"audio/mp3"); 

  // Option 1
  //dec_m4a.setSampleSizesBuffer(sizes_buffer);

  // Option 2
  // set custom buffer to optimize the memory usage
  // buffer_file = SD.open("/home/pschatzmann/tmp.tmp", O_RDWR | O_CREAT);
  // file_buffer.begin(buffer_file);
  // dec_m4a.setSampleSizesBuffer(file_buffer);

  // Option 3
  // WiFi.begin("ssid","pwd");
  // while ( WiFi.status() != WL_CONNECTED) {
  //   Serial.print(".");
  // }
  // if (!client.connect(IPAddress(192,168,1,10),6379)){
  //   Serial.println("redis error");
  //   stop();
  // }
  // dec_m4a.setSampleSizesBuffer(redis);
 
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