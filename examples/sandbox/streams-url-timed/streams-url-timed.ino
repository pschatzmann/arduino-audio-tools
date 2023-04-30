/**
 * 
*/
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
//#include "AudioLibs/AudioKit.h"


URLStream url("ssid","password");
I2SStream i2s; // or any other output of decoded stream (e.g. AudioKitStream)
TimedStream timed(i2s, 10, 20); // timed output with times
EncodedAudioStream dec(&timed, new MP3DecoderHelix()); // Decoding stream
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

  // start timed
  //timed.setStartSecond(10);
  //timed.setEndSecond(20);
  timed.begin();

  // setup I2S based on sampling rate provided by decoder
  dec.begin();

// mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  copier.copy();
}
