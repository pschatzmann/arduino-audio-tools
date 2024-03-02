/**
 * @file streams-url_mp3-metadata2.ino
 * @author Phil Schatzmann
 * @brief read MP3 stream from url and output metadata and audio! 
 * The used mp3 file contains ID3 Metadata!
 * @date 2021-11-07
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"

//                            -> EncodedAudioStream -> I2SStream
// URLStream -> MultiOutput -|
//                            -> MetaDataOutput

URLStream url("ssid","password");
MetaDataOutput out1; // final output of metadata
I2SStream i2s; // I2S output
EncodedAudioStream out2dec(&i2s, new MP3DecoderHelix()); // Decoding stream
MultiOutput out;
StreamCopy copier(out, url); // copy url to decoder

// callback for meta data
void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);
}

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup multi output
  out.add(out1);
  out.add(out2dec);

  // setup input
  url.begin("https://pschatzmann.github.io/Resources/audio/audio.mp3","audio/mp3");

  // setup metadata
  out1.setCallback(printMetaData);
  out1.begin(url.httpRequest());

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  out2dec.begin();

}

void loop(){
  copier.copy();
}
