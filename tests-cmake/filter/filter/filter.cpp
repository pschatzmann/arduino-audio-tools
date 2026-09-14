// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/PortAudioStream.h"

// define FIR filter
float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};
// 
uint16_t sample_rate=44100;
uint8_t channels = 2;                                             // The stream will have 2 channels 
WhiteNoiseGenerator<int16_t> noise(32000);                        // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(noise);                   // Stream generated from sine wave
FilteredStream<int16_t, float> in_filtered(in_stream, channels);  // Defiles the filter as BaseConverter
PortAudioStream out;                                              // Output to Desktop
StreamCopy copier(out, in_stream, 1012);                              // copies sound to out


void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  auto cfg = noise.defaultConfig();
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = 16;
  noise.begin(cfg);
  in_stream.begin();

  // setup filters for all available channels
  in_filtered.setFilter(0, new FIR<float>(coef));
  in_filtered.setFilter(1, new FIR<float>(coef));
  
  out.begin(cfg);
}

void loop(){
    copier.copy();
}

