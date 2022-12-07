/**
 * @file streams-i2s-webserver_wav.ino
 *
 *  This sketch reads sound data from I2S. The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3LAME.h"

AudioEncoderServer server(new MP3EncoderLAME(),"ssid","password");  

I2SStream i2sStream;    // Access I2S as stream
ConverterFillLeftAndRight<int16_t> filler(RightIsEmpty); // fill both channels

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto config = i2sStream.defaultConfig(RX_MODE);
  config.i2s_format = I2S_MSB_FORMAT;
  config.sample_rate = 22050;
  config.channels = 2;
  config.bits_per_sample = 16;
  i2sStream.begin(config);
  Serial.println("I2S started");

  // start data sink
  server.begin(i2sStream, config, &filler);
}

// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
