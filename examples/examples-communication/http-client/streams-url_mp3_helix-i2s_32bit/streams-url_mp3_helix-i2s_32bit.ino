/**
 * @file streams-url_mp3-i2s_24bit.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S. The data is converted from
 * 16 to 32 bit
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"


URLStream url("ssid","password");
I2SStream i2s; // final output of decoded stream
NumberFormatConverterStream nfc(i2s);
EncodedAudioStream dec(&nfc, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // convert 16 bits to 32, you could also change the gain
  nfc.begin(16, 32); 

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  // you could define e.g your pins and change other settings
  //config.pin_ws = 10;
  //config.pin_bck = 11;
  //config.pin_data = 12;
  //config.mode = I2S_STD_FORMAT;
  //config.bits_per_sample = 32; // we coult do this explicitly
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

// mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  copier.copy();
}
