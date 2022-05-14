/**
 * @file streams-url_flac-i2s.ino
 * @author Phil Schatzmann
 * @brief The FLAC decoder using our regular interface logic
 * @version 0.1
 * @date 2022-05-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioCodecs/CodecFLAC.h"

const char* ssid = "ssid";
const char* pwd = "password";
URLStream url(ssid, pwd);
I2SStream i2s;
EncodedAudioStream dec(&i2s, new FLACDecoder()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  i2s.begin(i2s.defaultConfig(TX_MODE));
  dec.begin();
  url.begin("http://www.lindberg.no/hires/test/2L-145_01_stereo_01.cd.flac");
}

void loop() {
  copier.copy();
}