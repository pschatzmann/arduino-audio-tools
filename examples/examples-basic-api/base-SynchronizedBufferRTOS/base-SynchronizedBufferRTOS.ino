/**
 * @file base-SynchronizedBufferRTOS.ino
 * @author Phil Schatzmann
 * @brief Data provider on core 0 with data consumer on core 1
 * @version 0.1
 * @date 2022-11-25
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "freertos-all.h" // https://github.com/pschatzmann/arduino-freertos-addons

SynchronizedBufferRTOS<int16_t> buffer(1024);
void doWrite(); // forward declaration 
Task writeTask("write",5000,10, doWrite);  // FreeRTOS task from addons

// create data and write it to buffer
void doWrite() {
    int16_t data[512];
    for (int j=0;j<512;j++){
        data[j]=j;
    }
    buffer.writeArray(data, 512);
}

void setup(){
    // Setup logging
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    // start on core 0
    writeTask.Start(0); 
}

// The loop runs on core 1: We read back the data
void loop(){
    int16_t data[512];
    uint64_t start = micros();
    buffer.readArray(data, 512);

    // process (verify) data
    int error=0;
    for (int j=0;j<512;j++){
        if(data[j]!=j){
          error++;
        }
    }

    // write result
    Serial.print("error: ");
    Serial.print(error);
    Serial.print(" runtime: ");
    Serial.println(micros()-start);
  }
