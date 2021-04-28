/**
 *  Record output from I2S and send it to a Bluetooth Speaker 
 */

#include "BluetoothA2DPSource.h"
#include "SoundTools.h"


BluetoothA2DPSource a2dp_source;
I2S<int16_t> i2s;

// callback used by A2DP to provide the sound data
int32_t get_sound_data(int16_t data[][2], int32_t len) {
  return i2s.read(data, len);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // start i2s input
  I2SConfig config = i2s.defaultConfig(RX);
  i2s.begin(config);

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("RadioPlayer", get_sound_data);  

}

// Arduino loop - repeated processing 
void loop() {
}
