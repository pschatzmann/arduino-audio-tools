
#include "AudioTools.h"

SineWaveGeneratorT<int16_t> sine_wave(32000);      // subclass of SoundGeneratorT with max amplitude of 32000
SineFromTableT<int16_t> sine_table(32000);         // subclass of SoundGeneratorT with max amplitude of 32000
FastSineGeneratorT<int16_t> sine_fast(32000);  

size_t measure(SoundGeneratorT<int16_t> *gen){
    uint64_t start = millis();
    size_t count = 0;
    for(int i=0;i<1000000;i++){
        int16_t s = gen->readSample();
    }
    uint64_t timeMs = millis()-start;
    // calculate samples per second
    return 1000000.0 / timeMs *1000;
}

const char* resultStr(const char* name, size_t count){
    static char msg[160];
    sprintf(msg, "%s: %ld", name, count);
    return msg;
}

void setup(){
    Serial.begin(115200);
    Serial.print("Number of samples per sec");
}

void loop(){
    Serial.print(resultStr("SineWaveGeneratorT", measure(&sine_wave)));
    Serial.print(" - ");
    Serial.print(resultStr("SineFromTableT", measure(&sine_table)));
    Serial.print(" - ");
    Serial.print(resultStr("FastSineGeneratorT", measure(&sine_fast)));
    Serial.println();
}