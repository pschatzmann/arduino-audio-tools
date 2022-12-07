// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"

uint16_t sample_rate=44100;
uint8_t channels = 2;                                             // The stream will have 2 channels 
SineWaveGenerator<int16_t> sine_wave(32000);                                           // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(sine_wave);                   // Stream generated from sine wave
PortAudioStream out;                                              // Output to Desktop
StreamCopy copier(out, in_stream);                                // copies sound to out
ChannelReducer<int16_t> reducer(channels,1);

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  sine_wave.begin(channels, sample_rate, N_B4);
  in_stream.begin();
  
  auto cfg1 = out.defaultConfig();
  cfg1.sample_rate = sample_rate;
  cfg1.channels = channels;
  cfg1.bits_per_sample = 16;
  out.begin(cfg1);
}

void loop(){
    copier.copy(reducer);
}

