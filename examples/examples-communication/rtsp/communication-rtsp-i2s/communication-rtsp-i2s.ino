/**
 * @file communication-rtsp-i2s.ino
 * @author Phil Schatzmann
 * @brief Demo for RTSP Client that is playing mp3:  tested with the live555 server with linux 
 * @version 0.1
 * @date 2022-05-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"  // https://github.com/pschatzmann/arduino-audio-tools
#include "AudioCodecs/CodecMP3Helix.h" // https://github.com/pschatzmann/arduino-libhelix
#include "RTSPSimpleClient.hh" // https://github.com/pschatzmann/arduino-live555.git

I2SStream i2s;
EncodedAudioStream out_mp3(&i2s, new MP3DecoderHelix());  // Decoding stream
RTSPSimpleClient rtsp;

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output: make sure we can buffer 1 decoded frame
  auto cfg_i2s = i2s.defaultConfig(TX_MODE);
  cfg_i2s.buffer_size = 1024;
  cfg_i2s.buffer_count = 10;
  i2s.begin(cfg_i2s);

  out_mp3.begin();
  
  // setup rtsp data source
  auto cfg = rtsp.defaultConfig();
  cfg.ssid = "ssid";
  cfg.password = "password";
  cfg.url = "rtsp://192.168.1.38:8554/test.mp3";
  cfg.output = &out_mp3;
  cfg.buffer_size = 1024*2; // space for 1 encoded frame
  //cfg.is_tcp = false; // use udp when false (default false)
  //cfg.is_blocking = false; // call singleStep in loop if false (default false)
  rtsp.begin(cfg);
}

void loop() {
  rtsp.singleStep();
}