/**
 * @file streams-url_wav-serial.ino
 * @author Phil Schatzmann
 * @brief decode WAV stream from rhasspy url and output it on I2S
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 
 */
#include "AudioTools.h"

// UrlStream -copy-> EncodedAudioStream -> I2S

URLStream url("ssid","password");
I2SStream i2s;                  // I2S stream 
WAVDecoder decoder;             // decode wav to pcm and send it to I2S
EncodedAudioStream out(&i2s, &decoder); // Decoder stream
StreamCopy copier(out, url);    // copy in to out


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.sample_rate = 16000; 
  config.bits_per_sample = 32;
  config.channels = 1;
  i2s.begin(config);

// rhasspy
  url.begin("http://192.168.1.37:12101/api/text-to-speech?play=false","audio/wav", POST, "text/plain","Hallo, my name is Alice");
}

void loop(){
  if (decoder) {
    copier.copy();
  } else {
    stop();
  }
}
