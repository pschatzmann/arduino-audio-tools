#define USE_SDFAT

#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"

using namespace audio_tools;  

const char *urls[] = {
  "http://centralcharts.ice.infomaniak.ch/centralcharts-128.mp3",
  "http://centraljazz.ice.infomaniak.ch/centraljazz-128.mp3",
  "http://centralrock.ice.infomaniak.ch/centralrock-128.mp3",
  "http://centralcountry.ice.infomaniak.ch/centralcountry-128.mp3",
  "http://centralfunk.ice.infomaniak.ch/centralfunk-128.mp3"
};

URLStream urlStream("SSID","password");
AudioSourceURL source(urlStream, urls, "audio/mp3");


void testUrl(){
    for (int j=-10;j<10;j++){
        Stream *out = source.setIndex(j);
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
AudioSourceSdFat sdsource(startFilePath, ext);

void testSD() {
    for (int j=-5;j<20;j++){
        Stream *out = sdsource.setIndex(j);
        Serial.printf("%d -> %d / %s \n", j, sdsource.index(), sdsource.toStr());
        if (out!=nullptr){
          assert(out->available()>0);
        }
    }
    Serial.println("--------------------------");
}

void testSDNext() {
    sdsource.begin();
    for (int j=0;j<20;j++){
        Stream *out = sdsource.nextStream(1);
        Serial.printf("%d -> %d / %s \n", j, sdsource.index(), sdsource.toStr());
        if (out!=nullptr){
          assert(out->available()>0);
        }
    }
    Serial.println("--------------------------");
}


void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);  
  testUrl();
  testSD();
  testSDNext();
}

void loop(){

}