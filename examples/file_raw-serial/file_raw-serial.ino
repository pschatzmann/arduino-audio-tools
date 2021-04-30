/**
 * @file file_raw-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/file_raw-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

File sound_file;
const char* file_name = "/audio.raw";
const int sd_ss_pin = 5;
const int32_t max_buffer_len = 512;
int32_t buffer[max_buffer_len][2];

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // Setup SD and open file
  SD.begin(sd_ss_pin);
  sound_file = SD.open(file_name, FILE_READ);
}

// Arduino loop - repeated processing 
void loop() {
  size_t len = sound_file.read((uint8_t*)buffer, max_buffer_len * sizeof(int16_t) * 2  ); 
  for (size_t j=0;j<len;j++){
    Serial.print(buffer[j][0]);
    Serial.print(", ");
    Serial.println(buffer[j][1]);
  }
}