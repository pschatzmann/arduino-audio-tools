/**
 * @file experiment-ads1015-serial.ino
 * @author Phil Schatzmann
 * @brief We can use the ADS1015 to provide analog signals. Unfortunatly it is not
 * suited for audio because it is too slow.
 * @version 0.1
 * @date 2022-12-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "Wire.h"
#include "Adafruit_ADS1015.h"
#include "AudioTools.h"

const int sample_rate = 3300;
const int buffer_size = 50;

Adafruit_ADS1015 ads1015(0x48);   
TimerAlarmRepeating sound_timer;
NBuffer<int16_t> buffer(buffer_size,3);  

// callback to record the sound data into a buffer
void write_sample(void *ptr) {
  sound_t sample = ads1015.readADC_Differential_0_1();
  buffer.write(sample);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // setup microphone
  ads1015.begin();  
  ads1015.setGain(GAIN_SIXTEEN); // GAIN_TWO GAIN_FOUR GAIN_EIGHT GAIN_SIXTEEN

  // start the timer to record the data
  long waitUs = 1000000 / sample_rate;
  sound_timer.start(write_sample, waitUs, US);

}

void printSampleRate() {
  static long next_time;
  if (millis()>next_time){
     next_time = next_time+1000;
     Serial.print("sample rate: ");
     Serial.println(buffer.sampleRate());
  }
}

void printSamples() {
  static sound_t array[50][2];
  size_t len = buffer.readFrames(array);
  for (int j=0;j<len;j++){
    Serial.print(array[j][0]);
    Serial.print(" ");
    Serial.println(array[j][1]);
  }
}

// Arduino loop - repeated processing
void loop() {
  // copy sound data from samples to I2S
  printSamples();
  printSampleRate();
  
}