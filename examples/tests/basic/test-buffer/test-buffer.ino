#include "AudioTools.h"

void test(BaseBuffer<int16_t> &b, const char* title){
    Serial.println(title);
    assert(b.isEmpty());
    for (int j=0;j<200;j++){
        assert(b.write(j));
    }
    assert(b.isFull());
    for (int j=0;j<200;j++){
        int16_t v = b.read();
        assert(v==j);
    }
    assert(b.isEmpty());
    int16_t array[200];
    for (int j=0;j<200;j++){
        array[j] = j;
    }
    assert(b.writeArray(array, 200)==200);
    assert(b.readArray(array,200)==200);
    for (int j=0;j<200;j++){
        assert(array[j]==j);
    }

    Serial.println("Test OK");
}

void setup(){
    SingleBuffer<int16_t> b1(200);
    RingBuffer<int16_t> b2(200);
    NBuffer<int16_t> b3(50,4);

    test(b1,"SingleBuffer");
    test(b2,"RingBuffer");
    test(b3,"NBuffer");

    Serial.println("Tests OK");
}

void loop(){}