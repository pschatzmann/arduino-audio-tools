#include "WiFi.h"
#include "AudioTools.h"



I2SStream i2s;                  // WAVDecoder neads I2S Output as stream
WAVDecoder decoder(i2s_stream); // Decoder writing to I2S Stream 
URLStream music;                // Music Stream
const size_t buffer_size = 512;
uint8_t buffer[buffer_size];
bool is_valid = true;


void setup(){
    Serial.begin(115200);
    // connect to WIFI
    WiFi.begin("network name", "password");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    // start I2S with the default configuration
    auto config = i2s.defaultConfig(TX_MODE);
    i2s.begin(config);

    // open music stream
    music.begin("https://www2.cs.uic.edu/~i101/SoundFiles/BabyElephantWalk60.wav");
    decoder.begin();
}

void loop(){
    if (music.available()>0){
        int len = music.readBytes(buffer, buffer_size);
        decoder.write(buffer, len);
    }  
}
