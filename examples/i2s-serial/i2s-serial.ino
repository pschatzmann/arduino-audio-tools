#include "Arduino.h"
#include "AudioTools.h"
#include "BluetoothA2DPSource.h"

using namespace audio_tools;  

/**
 * @brief We use I2S as input and send the data to A2DP
 * To test we use a INMP441 microphone. 
 */ 

I2S<int32_t> i2s;
const size_t max_buffer_len = 512;
int32_t buffer[max_buffer_len][2];

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  I2SConfig<int32_t> config = i2s.defaultConfig(RX_MODE);
  config.i2s.sample_rate = 16000;
  i2s.begin(config);
}

// Arduino loop - repeated processing 
void loop() {
  size_t len = i2s.read(buffer, max_buffer_len); 

  for (int j=0;j<len;j++){
    Serial.print((int32_t)buffer[j][0]);
    Serial.print(", ");
    Serial.println((int32_t)buffer[j][1]);
  }
}