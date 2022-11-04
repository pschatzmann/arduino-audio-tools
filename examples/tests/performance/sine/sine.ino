
#include "AudioTools.h"

SineWaveGenerator<int16_t> sine_wave(32000);      // subclass of SoundGenerator with max amplitude of 32000
SineFromTable<int16_t> sine_table(32000);         // subclass of SoundGenerator with max amplitude of 32000
FastSineGenerator<int16_t> sine_fast(32000);  
int sec = 5;

size_t measure(int sec, SoundGenerator<int16_t> *gen){
    uint64_t end = millis()+(1000*sec); // 10 seconds
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
    size_t result1 = measure(sec, &sine_wave);
    size_t result2 = measure(sec, &sine_table);
    size_t result3 = measure(sec, &sine_fast);

    Serial.print(resultStr("SineWaveGenerator", result1));
    Serial.print(" - ");
    Serial.print(resultStr("SineFromTable", result2));
    Serial.print(" - ");
    Serial.print(resultStr("FastSineGenerator", result3));
    Serial.println();
}