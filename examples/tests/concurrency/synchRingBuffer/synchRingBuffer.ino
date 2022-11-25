#include "AudioTools.h"
#include "AudioLibs/SynchronizedBuffer.h"
#include "freertos-all.h"

audio_tools::Mutex mutex;
RingBuffer<int16_t> rbuffer(512*4);
SynchronizedBuffer<int16_t> buffer(rbuffer, mutex);

Task writeTask("write",3000,10, [](){ 
    int16_t data[512];
    for (int j=0;j<512;j++){
        data[j]=j;
    }
    buffer.writeArray(data, 512);
});

Task readTask("read",3000,10, [](){ 
    int16_t data[512];
    uint64_t start = micros();
    buffer.readArray(data, 512);
    for (int j=0;j<512;j++){
        assert(data[j]==j);
    }
    Serial.print("runtime: ");
    Serial.println(micros()-start);
});


void setup(){
    Serial.begin(115200);
    writeTask.Start(0);
    writeTask.Start(1);
}

void loop(){
    delay(1000);
}