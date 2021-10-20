// Add this in your sketch or change the setting in AudioConfig.h
#define USE_A2DP

#include "Arduino.h"
#include <SPI.h>
#include <SD.h>
#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

// configuration
const int sd_ss_pin = 4;

// common data
BluetoothA2DPSink a2dp_sink;
File sound_file;
AACEncoder *encoder;

// detrmines the next file name
char* nextFileName(){
    static int idx;
    static char[10] result;
    sprintf(result,"F%000d", ++idx);
    return result;
}

// opens a new file 
void openFile() {
    Serial.println("Recording next file...");
    // save and cleanup of last file
    sound_file.close();
    if encoder != nullptr delete encoder;

    // new file
    dataFile = SD.open(nextFileName(), FILE_WRITE);
    encoder = new AACEncoder(dataFile);
    encoder->begin();
}

// Get the audio data from A2DP to save it in a file
void read_data_stream(const uint8_t *data, uint32_t length) {
    if (encoder!=nullptr) encoder.write(data, len);
}

// Handle meta data changes
void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
    Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
    if (data1==01 && strncmp(last, (char*)data2, 100)!=0){
      openFile();
    }
    if (data2) strncpy(last,(char*)data2,100);
}

void setup() {
    Serial.begin(115200);

    // setup A2DP
    a2dp_sink.set_stream_reader(read_data_stream, false);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.start("AAC-Recorder");

    // Setup SD 
    SD.begin(sd_ss_pin);
}


void loop() {
}