/**
 * @file test-codec-fdk_aac.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S on audiokit using the FDK decoder
 * 
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */


#include "AudioTools.h"
#include "AudioCodecs/CodecAACFDK.h"
#include "AudioLibs/AudioKit.h"


URLStream url("ssid","password");  // or replace with ICYStream to get metadata
AudioKitStream i2s; // final output of decoded stream
EncodedAudioStream dec(&i2s, new AACDecoderFDK()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder

void loop1(void*) {
  // setup decoder
  dec.begin();
  // processing loop
  while(true){
    copier.copy();
    delay(1);
  }
}

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // aac radio
  url.begin("http://peacefulpiano.stream.publicradio.org/peacefulpiano.aac","audio/aac");

  int stack = 100000;
  xTaskCreate(loop1,"loopTask", stack, nullptr,1, nullptr);

}

void loop(){
}
