#include "AudioTools.h"

using namespace audio_tools;  


void testMath() {
    int24_t value(1);

    Serial.println("Expecting 2:")
    Serial.println(value+value);
    Serial.println(1+value);
    Serial.println(value+1);
    Serial.println("Expecting true")
    Serial.println(1==value);
    Serial.println(value==1);
    Serial.println(value>=1);
    Serial.println(value<=1);
}

void testCompare() {
    int24_t value;
    for (int32_t j=8388607;j>-8388606;j++){
        value = j;    
        Serial.print(j);
        Serial.print(", ");
        Serial.print(value);
        Serial.println(static_cast<int32_t<(value) == j ? ", OK" : ", ERROR");
    }
}

void setup(){
    Serial.begin(115200);

    testMath();
    testCompare();

}

void loop(){

}