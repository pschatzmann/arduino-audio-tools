
#include "AudioTools.h"
#include "AudioBasic/Collections/BitVector.h"
#ifndef HAS_IOSTRAM
#error processor does nos support this functionality
#endif
BitVector bv;

void printBitVector(){
    for (int j=0;j<bv.size();j++){
        Serial.print(bv[j]?1:0);
    }
    Serial.println();
}

void test(){
    printBitVector();
    bv.set(80,true);
    printBitVector();
    bv.set(0,true);
    printBitVector();
    bv.set(16,true);
    printBitVector();
    bv.set(17,true);
    printBitVector();
    bv.set(0,false);
    printBitVector();
}


void setup(){
    Serial.begin(115200);
    test();
}

void loop(){

}