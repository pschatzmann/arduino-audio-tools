/**
 * @file streams-url_mp3-out.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it with the help of PWM
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


URLStream url("ssid","password");
PWMAudioOutput out; // final output of decoded stream
EncodedAudioStream dec(&out, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup out
  auto config = out.defaultConfig(TX_MODE);
  //config.resolution = 8;  // must be between 8 and 11 -> drives pwm frequency (8 is default)
  // alternative 1
  //config.start_pin = 3;
  // alternative 2
  int pins[] = {22, 23};
  // alternative 3
  //Pins pins = {3};
  //config.setPins(pins); 
  out.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

// mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  copier.copy();
}
