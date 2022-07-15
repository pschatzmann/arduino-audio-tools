/**
 * @file streams-url_mp3-audiokit.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S on audiokit
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioCodecs/CodecMP3MAD.h"

#include "AudioLibs/AudioKit.h"


URLStream url("ssid","password");  // or replace with ICYStream to get metadata
AudioKitStream i2s; // final output of decoded stream
EncodedAudioStream dec(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  config.channels = 1;
  config.sample_rate = 24000;
  i2s.begin(config);

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

  // mp3 radio
  url.begin("https://pschatzmann.github.io/arduino-audio-tools/resources/short-mp3.mp3","audio/mp3");

}

void loop(){
  if (copier.available()) { // have tried copier.available and audioFile.available
    copier.copy();
  } else {

    helix.end(); // flush output
    i2s.end();
    stop();
  }
}
