/**
 *  Test sampling rate
 */

#include "AudioTools.h"

unsigned long count;
unsigned long start;
const int sample_rate = 20000;  
TimerAlarmRepeating sound_timer(1);
SineWaveGenerator<int16_t> sine(100); // values between -100 to 100

// callback to record the sound data into a buffer
void process_sample(void *ptr) {
   if (count==0){
          start = millis();
          count = 0;
   }
   sine.readSample(); // calculate sample to check if this has an impact
   count++;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  disableCore0WDT();
  sine.begin(sample_rate, E3);  
 
  long waitUs = (1000000 / sample_rate) ;
  sound_timer.start(process_sample, waitUs, US);

}

// Arduino loop - repeated processing: we calculate the sampling rate every 1000 samples
void loop() {
    if (count % 1000 == 0){
        unsigned long run_time = millis()-start;
        if (run_time>0) {
              Serial.print("sampling rate: ");
              Serial.println(count / run_time * 1000);
        }
    }
}