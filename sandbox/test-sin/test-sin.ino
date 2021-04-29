/**
 *  Generate a sine wave to test the TimerAlarmRepeating and UpSampler
 */

#include "SoundTools.h"

const int sample_rate = 44100;  
const int buffer_size = 512;  
const int buffer_count = 10;
TimerAlarmRepeating sound_timer;
SineWaveGenerator<int16_t> sine(100); // -100 to 100
NBuffer<int16_t> buffer(buffer_size, buffer_count);

// callback to record the sound data into a buffer
void write_sample(void *ptr) {
  int16_t sample = sine.readSample();  
  buffer.write(sample);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  //disableCore0WDT();
  sine.begin(sample_rate, A3);  

  // start the timer to record the data
  long waitUs = 1000000 / sample_rate;
  sound_timer.start(write_sample, waitUs, TimerAlarmRepeating::US);
}


// Arduino loop - repeated processing: We check if we find any gaps in the buffered data (with a big difference between 2 subsequent samples)
void loop() {
  int available = buffer.available();
  if (available>=buffer_size){
    int16_t sample = buffer.read();
    int16_t last_sample;
    uint16_t gap_count = 0;
	  for (int j=0;j<available-1;j++){   
      last_sample = sample;   
      sample = buffer.read();
      if (abs(sample - last_sample)>1){
        gap_count++;
      }
	  }
    Serial.print("Number of gaps: ");
    Serial.println(gap_count);
  }
}