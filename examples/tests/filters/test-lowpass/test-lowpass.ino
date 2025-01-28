#include "AudioTools.h"

AudioInfo info(44100, 2 , 16);
VolumeMeter meter;
FilteredStream<int16_t, float> filtered(meter);
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> sine_stream(sine);
StreamCopy copier(filtered, sine_stream);
float filter_freq = 8000;

// Filter: or replace with other filters
LowPassFilter<float> filter1(filter_freq, info.sample_rate);
LowPassFilter<float> filter2(filter_freq, info.sample_rate);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  sine.begin(info, 0);
  if (!sine_stream.begin(info)){
    Serial.println("Generator error");
    stop();
  }
  
  if (!filtered.begin(info)){
    Serial.println("filter error");
    stop();
  }
  filtered.setFilter(0, filter1);
  filtered.setFilter(1, filter2);

  if (!meter.begin(info)){
    Serial.println("meter error");
    stop();
  }

}

void loop() {
  for (int freq= 20; freq<20000; freq+=100){
      meter.clear();
      sine.setFrequency(freq);
      copier.copyN(10);  
      Serial.print(freq);
      Serial.print(": ");
      Serial.println(meter.volumeRatio());      
  }
  stop(); 
}