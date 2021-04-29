
#include "AudioTools.h"
#include <SPI.h>
#include <SD.h>

I2S i2s; // I2S output destination
File sound_file;
AACDecoder decoder(i2s);
const int sd_ss_pin = 4;
const char* file_name = "audio.aac"
uint8_t buffer[512];


// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // Setup SD and open file
  SD.begin(sd_ss_pin);
  sound_file = SD.open(file_name, FILE_READ);

  i2s.begin();
  decoder.begin();
}


// Arduino loop - repeated processing 
void loop() {
    if (sound_file.available()>0){
        int len = sound_file.readBytes(buffer, 512);
        decoder.write(buffer, len);
    }

}
