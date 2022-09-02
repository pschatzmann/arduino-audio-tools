/**
 * @file test-player.ino
 * @author Phil Schatzmann
 * @brief 
 * @version 0.1
 * @date 2022-04-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// install https://github.com/greiman/SdFat.git

#include "AudioTools.h"
#include "AudioLibs/AudioSourceSDFAT.h"


const char *urls[] = {
  "http://stream.srg-ssr.ch/m/rsj/mp3_128",
  "http://stream.srg-ssr.ch/m/drs3/mp3_128",
  "http://stream.srg-ssr.ch/m/rr/mp3_128",
  "http://sunshineradio.ice.infomaniak.ch/sunshineradio-128.mp3",
  "http://streaming.swisstxt.ch/m/drsvirus/mp3_128"
};

URLStream urlStream("SSID","password");
AudioSourceURL source(urlStream, urls, "audio/mp3");


void testUrl(){
    for (int j=-10;j<10;j++){
        Stream *out = source.selectStream(j);
        Serial.printf("%d -> %d / %s \n", j, source.index(), source.toStr());
        if (out!=nullptr){
          delay(500);
          assert(out->available()>0);
        }
    }
    Serial.println("--------------------------");
}

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSDFAT sdSource(startFilePath, ext);

void testSD() {
    sdSource.setPath("/");
    sdSource.begin();

    for (int j=-5;j<20;j++){
        Stream *out = sdSource.selectStream(j);
        Serial.printf("%d -> %d / %s \n", j, sdSource.index(), sdSource.toStr());
        if (out!=nullptr){
          assert(out->available()>0);
        }
    }
    Serial.println("--------------------------");
}

void testSDNext() {
    sdSource.setPath("/");
    sdSource.begin();
    for (int j=0;j<20;j++){
        Stream *out = sdSource.nextStream(1);
        Serial.printf("%d -> %d / %s \n", j, sdSource.index(), sdSource.toStr());
        if (out!=nullptr){
          assert(out->available()>0);
        }
    }
    Serial.println("--------------------------");
}


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);  
  //testUrl();
  testSD();
  testSDNext();
}

void loop(){

}