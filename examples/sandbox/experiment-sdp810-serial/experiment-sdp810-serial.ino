/**
 *  Record output from SDP810 infrasonic microphone and send it to the Serial output 
 * 
 */

#include <Wire.h>
#include <SDPSensors.h> //https://github.com/UT2UH/SDP3x-Arduino/tree/SDP8x
#include <AudioTools.h>

using namespace audio_tools;
typedef int16_t sound_t; // sound will be represented as int16_t

const int sample_rate = 1024;
const int buffer_size = 64;
bool status;
int16_t pressure;

SDP8XX Mic = SDP8XX(Address5, DiffPressure, Wire);   
TimerAlarmRepeating samplingTimer(1);
NBuffer<int16_t> buffer(buffer_size, 3);

// callback to record the sound data into a buffer
void writeSample(void *ptr) {
  status = Mic.readMeasurement(&pressure, NULL, NULL);
  sound_t sample = pressure;
  buffer.write(sample);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);

  // setup SDP810 infrasound microphone
  Wire.begin();
  Mic.begin();
  Mic.startContinuous(false);

  // start the timer to record the data
  long waitUs = 1000000 / sample_rate;
  samplingTimer.start(writeSample, waitUs, US);

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
  static sound_t array[buffer_size][2];
  size_t len = buffer.readFrames(array);
  for (int j=0;j<len;j++){
    Serial.print(array[j][0]);
    Serial.print(" ");
    Serial.println(array[j][1]);
  }
}

// Arduino loop - repeated processing
void loop() {
  // copy sound data from samples to Serial
  printSamples();
  printSampleRate();
}