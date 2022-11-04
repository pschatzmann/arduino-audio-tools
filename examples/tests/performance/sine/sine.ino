
#include "AudioTools.h"

SineWaveGenerator<int16_t> sine_wave(32000);      // subclass of SoundGenerator with max amplitude of 32000
SineFromTable<int16_t> sine_table(32000);         // subclass of SoundGenerator with max amplitude of 32000
FastSineGenerator<int16_t> sine_fast(32000);  
int sec = 5;

size_t measure(int sec, SoundGenerator<int16_t> *gen){
    uint64_t end = millis()+(1000*sec); 
    size_t count;
    while(millis()<end){
        int16_t s = gen->readSample();
        count++;
    }
    return count;
}

const char* resultStr(const char* name, size_t count){
    static char msg[160];
    sprintf(msg, "%s: %ld", name, count);
    return msg;
}

void setup(){
    Serial.begin(115200);
    Serial.print("Number of call in ");
    Serial.print(sec);
    Serial.println(" seconds:");
    Serial.println();
}

void loop(){
    Serial.print(resultStr("SineWaveGenerator", measure(sec, &sine_wave)));
    Serial.print(" - ");
    Serial.print(resultStr("SineFromTable", measure(sec, &sine_table)));
    Serial.print(" - ");
    Serial.print(resultStr("FastSineGenerator", measure(sec, &sine_fast)));
    Serial.println();
}