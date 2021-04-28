
#include "BluetoothA2DPSource.h"
#include <SPI.h>
#include <SD.h>

BluetoothA2DPSource a2dp_source;
File sound_file;
const char* file_name = "audio.raw";
const int sd_ss_pin = 4;

// callback used by A2DP to provide the sound data
int32_t get_sound_data(Channels* data, int32_t len) {
  // the data in the file must be in int16 with 2 channels 
  return sound_file.read((uint8_t*)data, len * sizeof(int16_t) * 2 );
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // Setup SD and open file
  SD.begin(sd_ss_pin);
  sound_file = SD.open(file_name, FILE_READ);

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("MyMusic", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
}
