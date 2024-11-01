/**
 * @file streams-url_flac-i2s.ino
 * @author Phil Schatzmann
 * @brief The FLAC decoder supports a streaming interface where we can directly assign a source stream
 * @version 0.1
 * @date 2022-05-13
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecVorbis.h"

const char* ssid = "ssid";
const char* pwd = "password";
URLStream url(ssid, pwd);
VorbisDecoder dec;
I2SStream i2s;

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  i2s.begin(i2s.defaultConfig(TX_MODE));

  url.begin("http://marmalade.scenesat.com:8086/bitjam.ogg","application/ogg");
  dec.setInput(url);
  dec.setOutput(i2s);
  dec.begin();
}

void loop() {
  dec.copy();
}