/**
 *  Record output from electrical guitar with a ADS1015 and send it to the internal DAC of the ESP32 
 *  see https://www.pschatzmann.ch/home/2021/03/02/arduino-based-guitar-pedals-with-standard-components-recording-sound/
 */

#include "Wire.h"
#include "Adafruit_ADS1015.h"
#include "BluetoothA2DPSource.h"
#include "SoundTools.h"

const int buffer_size = 512;  
const int buffer_count = 10;
const int sampling_rate = 44100;

Adafruit_ADS1015 ads1015(0x48);   
TimerAlarmRepeating sound_timer;
NBuffer<int16_t> buffer(buffer_size, buffer_count);  
I2S<int16_t> i2s;


// callback to record the sound data into a buffer
void write_sample(void *ptr) {
	sound_t sample = ads1015.readADC_Differential_0_1();
	buffer.write(sample);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  ads1015.begin();  
  ads1015.setGain(GAIN_SIXTEEN); // GAIN_TWO GAIN_FOUR GAIN_EIGHT GAIN_SIXTEEN

  // start i2s
  i2s.begin(0, sample_rate);

  // start the timer to record the data
 long waitUs = 1000000 / sample_rate;
  sound_timer.start(write_sample, waitUs, TimerAlarmRepeating::US);
}

void writeI2S(){
  static sound_t array[50][2];
  // copy sound data from samples to I2S
  size_t len = buffer.readFrames(array);
  if (len>0){
    i2s.write(array, len);
  }  
  
}

// Arduino loop - repeated processing
void loop() {
  writeI2S();
}