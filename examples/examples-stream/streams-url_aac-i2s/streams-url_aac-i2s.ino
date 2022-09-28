/**
 * @file streams-url_aac-i2s.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecAACHelix.h"


URLStream url("ssid","password");
I2SStream i2s; // final output of decoded stream
EncodedAudioStream dec(&i2s, new AACDecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  // you could define e.g your pins and change other settings
  //config.pin_ws = 10;
  //config.pin_bck = 11;
  //config.pin_data = 12;
  //config.mode = I2S_STD_FORMAT;
  i2s.begin(config);

  // aac radio
  dec.setNotifyAudioChange(i2s);
  url.begin("http://peacefulpiano.stream.publicradio.org/peacefulpiano.aac","audio/aac");

  // initialize decoder
  dec.begin();

}

void loop(){
  copier.copy();
}
