#include "AudioTools.h"

using namespace audio_tools;  

void setup(){
    int24_t value;
    Serial.begin(115200);

    for (int32_t j=8388607;j>-8388606;j++){
        value = j;    
        Serial.print(j);
        Serial.print(", ");
        Serial.print((int32_t)value);
        Serial.println((int32_t)value == j ? ", OK" : ", ERROR");
    }

}

void loop(){

}