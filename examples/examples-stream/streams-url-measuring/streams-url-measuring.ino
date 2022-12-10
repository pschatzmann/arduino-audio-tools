 /**
 * @file streams-url_mp3-measuring.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and measure bytes per second of decoded data
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

URLStream url("ssid","password");  // or replace with ICYStream to get metadata
MeasuringStream out(50, &Serial); // final output of decoded stream
EncodedAudioStream dec(&out, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);  

  dec.begin();

  // mp3 radio
  url.begin("http://direct.fipradio.fr/live/fip-midfi.mp3","audio/mp3");

}

void loop(){
  copier.copy();
}