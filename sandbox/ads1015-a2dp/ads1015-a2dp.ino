/**
 *  Record output from electrical guitar with a ADS1015 and send it to a Bluetooth Speaker 
 *  see https://www.pschatzmann.ch/home/2021/03/02/arduino-based-guitar-pedals-with-standard-components-recording-sound/
 */

#include "Wire.h"
#include "Adafruit_ADS1015.h"
#include "BluetoothA2DPSource.h"
#include "Buffer.h"
#include "TimerAlarmRepeating.h"

const int buffer_size = 512;  
const int buffer_count = 10;
const int sampling_rate = 44100;
bool active = false;

NBuffer<int16_t> buffer(buffer_size, buffer_count);
Adafruit_ADS1015 ads1015(0x48);   
TimerAlarmRepeating sound_timer;
BluetoothA2DPSource a2dp_source;

// A2DP callback to provide the sound data from buffer
int32_t get_sound_data(Channels *data, int32_t len) {  
  active = true;
  return buffer.readFrames( (int16_t (*)[2]) data, len);
}

// Timer callback to record the sound data into a buffer
void write_sample(void *ptr) {
  if (active) {
    sound_t sample = ads1015.readADC_Differential_0_1();
    buffer.write(sample);
  }
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  ads1015.begin();  
  ads1015.setGain(GAIN_SIXTEEN); 

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.setNVSInit(true);
  a2dp_source.start("RadioPlayer", get_sound_data);  

  // start the timer to record the data
  long waitUs = 1000000 / sampling_rate;  
  sound_timer.start(write_sample, waitUs, TimerAlarmRepeating::US);
}


void printSampleRate() {
  static long next_time;
  if (millis()>next_time){
     next_time = next_time+1000;
     Serial.print("sample rate: ");
     Serial.println(buffer.sampleRate());
  }
}

// Arduino loop - repeated processing - not used
void loop() {
  printSampleRate();  
}