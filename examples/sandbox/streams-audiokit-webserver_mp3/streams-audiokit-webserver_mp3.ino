 /**
 * @file streams-audiokit-webserver_mp3.ino
 *
 *  This sketch reads sound data from I2S. The result is provided as MP3 stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3LAME.h"
#include "AudioLibs/AudioKit.h"

AudioEncoderServer server(new MP3EncoderLAME(),"SSID","password");  
AudioKitStream i2sStream;    // Access I2S as stream

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto config = i2sStream.defaultConfig(RX_MODE);
  config.sample_rate = 22050;
  config.channels = 2;
  config.bits_per_sample = 16;
  config.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  i2sStream.begin(config);
  Serial.println("I2S started");

  // start data sink
  server.begin(i2sStream, config);
}

// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}