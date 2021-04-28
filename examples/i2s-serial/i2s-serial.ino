#include "Arduino.h"
#include "SoundTools.h"
#include "BluetoothA2DPSource.h"

using namespace sound_tools;  

/**
 * @brief We use a ADS1015 I2S microphone as input and send the data to A2DP
 * Unfortunatly the data type from the microphone (int32_t)  does not match with the required data type by A2DP (int16_t),
 * so we need to convert.
 */ 

I2S<int32_t> i2s;
const int32_t max_buffer_len = 512;
int32_t buffer[max_buffer_len][2];

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  i2s.begin(i2s.defaultConfig(RX_MODE));
}

// Arduino loop - repeated processing 
void loop() {
  size_t len = i2s.read(buffer, max_buffer_len); 
  for (int j=0;j<len;j++){
    Serial.print(buffer[j][0]);
    Serial.print(", ");
    Serial.println(buffer[j][1]);
  }
}