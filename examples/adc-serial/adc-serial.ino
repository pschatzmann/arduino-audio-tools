/**
 * @file adc-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/adc-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "Arduino.h"
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"

using namespace audio_tools;  

/**
 * @brief We use a mcp6022 analog microphone on GPIO34 and write it to Serial
 */ 

ADC adc;
const int32_t max_buffer_len = 512;
int16_t buffer[max_buffer_len][2];
// The data has a center of around 26427, so we we need to shift it down to bring the center to 0
FilterScaler<int16_t> scaler(1.0, -26427, 32700 );

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input with default configuration
  Serial.println("starting I2S-ADC...");
  adc.begin(adc.defaultConfig());

}

// Arduino loop - repeated processing 
void loop() {
  size_t len = adc.read(buffer, max_buffer_len); 
  // move center to 0 and scale the values
  scaler.process(buffer, len);

  for (int j=0;j<len;j++){
    Serial.print(buffer[j][0]);
    Serial.print(", ");
    Serial.println(buffer[j][1]);
  }
}