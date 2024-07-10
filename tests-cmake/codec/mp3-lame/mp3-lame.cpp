// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioCodecs/CodecMP3LAME.h"
//#include <stdlib.h>  // for rand

using namespace audio_tools;  

HexDumpStream out(Serial);
MP3EncoderLAME mp3(out);
AudioInfoLAME info;
int16_t buffer[512];

void setup() {
    Serial.begin(115200);

    info.channels = 1;
    info.sample_rate = 16000;
    mp3.begin(info);

    Serial.println("starting...");

}

void loop() {
    for (int j=0;j<512;j++){
        buffer[j] = (rand() % 100) - 50;         
    }
    if (mp3.write((uint8_t*)buffer, 512*sizeof(int16_t))){
        out.flush();
        Serial.println("512 samples of random data written");
    }
}
