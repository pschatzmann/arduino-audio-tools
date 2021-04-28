#include "Arduino.h"
#include "SoundTools.h"

using namespace sound_tools;  

/**
 * @brief We use a ADS1015 I2S microphone as input and send the data to the serial port
 */ 

I2S<int32_t> i2s;
const int32_t max_buffer_len = 512;
int32_t buffer[max_buffer_len][2];


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  // start i2s input with default configuration
  Serial.println("starting I2S...");
  I2SConfig<int32_t> cfg = i2s.defaultConfig(RX_MODE);
  cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2s.begin(cfg);

}

// Arduino loop - repeated processing 
void loop() {

   // the microphone provides data in int32_t;
   size_t result = i2s.read(buffer, max_buffer_len);
   for (size_t j=0;j<result;j++){
     Serial.print(buffer[j][0]);
     Serial.print(", ");
     Serial.println(buffer[j][1]);
   }

}