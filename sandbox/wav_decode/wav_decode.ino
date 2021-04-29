#include "AudioWAV.h"
#include "AudioTools.h"

I2S i2s; // I2S output destination
WAVDecoder decoder(i2s);  
UrlStream music;

void setup(){
    Serial.begin(115200);
    // connect to WIFI
    WiFi.begin("network name", "password");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    music.begin("https://www2.cs.uic.edu/~i101/SoundFiles/BabyElephantWalk60.wav");
    i2s.begin();
    decoder.begin();
}

void loop(){
    static uint8_t buffer[512];
    if (music.available()>0){
        int len = music.readBytes(buffer, 512);
        decoder.write(buffer, len);
    }
}