/**
 * @file test-converter.ino
 * @author your name (you@domain.com)
 * @brief We read 32 bit 1 channel values from i2s and convert them to 16 bits 
 * @version 0.1
 * @date 2022-03-03
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"


I2SStream i2sStream; // Access I2S as stream
FormatConverter<int32_t,int16_t> converter(&NumberConverter::convertFrom32To16);
ConverterFillLeftAndRight<int32_t> bothChannels(LeftIsEmpty);

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    auto cfg = i2sStream.defaultConfig(RX_MODE);
    cfg.i2s_format = I2S_STD_FORMAT; // or try with I2S_LSB_FORMAT
    cfg.bits_per_sample = 32;
    cfg.channels = 2;
    cfg.sample_rate = 44100;
    cfg.is_master = true;
     // this module nees a master clock if the ESP32 is master
    i2sStream.begin(cfg);
}

const size_t max_buffer_len = 150;
const size_t max_buffer_bytes = max_buffer_len*sizeof(int32_t)*2;
uint8_t buffer[max_buffer_bytes]={0};

// callback used by A2DP to provide the sound data - usually len is 128 2 channel int16 frames
int32_t get_sound_data(int32_t len) {
  size_t req_bytes = min(max_buffer_bytes, len*2*sizeof(int16_t));
 
  // the microphone provides data in int32_t -> we read it into the buffer of int32_t data so *2
  size_t result_bytes = i2sStream.readBytes((uint8_t*)buffer, req_bytes*2);

  // we have data only in 1 channel but we want to fill both
  bothChannels.convert((uint8_t*)buffer, result_bytes);
  // convert buffer to int16 for A2DP
  converter.convert(buffer, (int16_t*)buffer, result_bytes);

  int count = result_bytes / sizeof(int32_t);
  int16_t *pt = (int16_t *)buffer;
  for (int j=0;j<count;j+=2){
      Serial.print(pt[j]);
      Serial.print(",");
      Serial.print(pt[j+1]);
      Serial.println();
  }
  
  return result_bytes;
}


// Arduino loop - copy data
void loop() {
    get_sound_data(128);
    
}