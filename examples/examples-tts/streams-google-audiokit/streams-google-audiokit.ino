/**
 * @file streams-google-audiokit.ino
 * @author Phil Schatzmann
 * @brief decode MP3 stream from url and output it on I2S on audiokit.
 * We are using the free google translate service to generate the mp3
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/pschatzmann/arduino-libhelix.git

#include "AudioTools.h"
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/AudioKit.h"


URLStream url("ssid","password");  
AudioKitStream i2s; // final output of decoded stream
EncodedAudioStream dec(&i2s, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, url); // copy url to decoder
StrExt query("http://translate.google.com/translate_tts?ie=UTF-8&tl=%1&client=tw-ob&ttsspeed=%2&q=%3");

const char* tts(const char* text, const char* lang="en", const char* speed="1"){
  query.replace("%1",lang);
  query.replace("%2",speed);

  StrExt encoded(text);
  encoded.urlEncode();
  query.replace("%3", encoded.c_str());
  return query.c_str();
}

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup i2s
  auto config = i2s.defaultConfig(TX_MODE);
  i2s.begin(config);

  // setup decoder
  dec.begin();

  // display url
  const char* url_str = tts("this is an english text");
  Serial.println(url_str);

  // generate mp3 with the help of google translate
  url.begin(url_str ,"audio/mp3");

}

void loop(){
  copier.copy();
}
