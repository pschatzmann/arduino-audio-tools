// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioCodecs/CodecAACFDK.h"
//#include <stdlib.h>  // for rand

using namespace audio_tools;  

HexDumpStream out(Serial);
AACEncoderFDK aac(out);
AudioBaseInfo info;
int16_t buffer[512];

void setup() {
    Serial.begin(115200);

    info.channels = 1;
    info.sample_rate = 16000;
    aac.begin(info);

    Serial.println("starting...");

}

void loop() {
    for (int j=0;j<512;j++){
        buffer[j] = (rand() % 100) - 50;         
    }
    if (aac.write((uint8_t*)buffer, 512*sizeof(int16_t))){
        out.flush();
        Serial.println("512 samples of random data written");
    }
}

