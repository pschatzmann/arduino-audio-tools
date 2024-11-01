/**
 * @file streams-url_aac-audiokit.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S on audiokit
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecAACHelix.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"


URLStream url("ssid","password");  // or replace with ICYStream to get metadata
AudioBoardStream i2s(AudioKitEs8388V1); // final output of decoded stream
EncodedAudioStream dec(&i2s, new AACDecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

  // aac radio
  url.begin("http://peacefulpiano.stream.publicradio.org/peacefulpiano.aac","audio/aac");

}

void loop(){
  copier.copy();
}
