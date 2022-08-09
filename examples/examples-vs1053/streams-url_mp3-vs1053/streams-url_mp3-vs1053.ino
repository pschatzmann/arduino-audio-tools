/**
 * @file streams-url_mp3-audiokit.ino
 * @author Phil Schatzmann
 * @brief Copy MP3 stream from url and output it on vs1053
 * @version 0.1
 * @date 2021-96-25
 * 
 * @copyright Copyright (c) 2021
 */

// install https://github.com/baldram/ESP_VS1053_Library.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"

#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4


URLStream url("ssid","password");  // or replace with ICYStream to get metadata
VS1053Stream vs1053(VS1053_CS,VS1053_DCS, VS1053_DREQ); // final output
StreamCopy copier(vs1053, url); // copy url to decoder


void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // setup vs1053
  vs1053.begin();

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  copier.copy();
}
