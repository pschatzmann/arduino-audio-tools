/**
 * @file base-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/base-i2s-a2dp/README.md
  * @copyright GPLv3
*/

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"

/**
 * @brief We use a INMP441 I2S microphone as input and send the data to A2DP
 * Unfortunatly the data type from the microphone (int32_t)  does not match with the required data type by A2DP (int16_t),
 * so we need to convert.
 */ 

BluetoothA2DPSource a2dp_source;
I2SStream i2s;
ConverterFillLeftAndRight<int16_t> bothChannels(LeftIsEmpty);
const size_t max_buffer_len = 150;
const int channels = 2;
const size_t max_buffer_bytes = max_buffer_len * sizeof(int16_t) * channels;
uint8_t buffer[max_buffer_bytes]={0};

// callback used by A2DP to provide the sound data - usually len is 128 2 channel int16 frames
int32_t get_sound_data(Frame* data, int32_t len) {
  size_t req_bytes = min(max_buffer_bytes, len*2*sizeof(int16_t));
  // the microphone provides data in int32_t -> we read it into the buffer of int32_t data so *2
  size_t result_bytes = i2s.readBytes((uint8_t*)buffer, req_bytes*2);
  // we have data only in 1 channel but we want to fill both
  return bothChannels.convert((uint8_t*)buffer, result_bytes);
}


// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto cfg = i2s.defaultConfig(RX_MODE);
  cfg.i2s_format = I2S_STD_FORMAT; // or try with I2S_LSB_FORMAT
  cfg.bits_per_sample = 16;
  cfg.channels = 2;
  cfg.sample_rate = 44100;
  cfg.is_master = true;
  i2s.begin(cfg);

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.set_auto_reconnect(false);
  a2dp_source.start("LEXON MINO L", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
  delay(1000);
}