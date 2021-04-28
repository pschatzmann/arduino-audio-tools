#include "Arduino.h"
#include <SPI.h>
#include <SD.h>
#include "SoundTools.h"

// configuration
const int minutes = 5;
const char* file_name = "audio.row";
const int buffer_len = 512;
const int sd_ss_pin = 4;

// common data
I2S<int16_t> i2s;
File sound_file;
int16_t buffer[buffer_len][2];


/// record I2S to a SD file
void record() {
    // Setup recording duration
    uint64_t end_time_ms = millis() + minutes * 60 * 1000 ;
    Serial.print("Recording now for ");
    Serial.print(minutes);
    Serial.println(" minutes...");

    // copy data
    int buffer_byte_count = buffer_len * 2 * sizeof(int16_t);
    while(end_time_ms>millis()){
        size_t len = i2s.read(buffer, buffer_len);
        file.write(buffer, buffer_byte_count);
    }
    sound_file.close();
    Serial.println("Recoring done");
}


void setup() {
    Serial.begin(115200);

    // setup I2S
    I2SConfig config = i2s.defaultConfig(RX);
    i2s.begin(config);

    // Setup SD and open file
    SD.begin(sd_ss_pin);
    sound_file = SD.open(file_name, FILE_WRITE);

}


void loop() {
}