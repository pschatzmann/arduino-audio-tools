#include "AudioTools.h"

uint16_t sample_rate=44100;
uint8_t from_channels = 1;                                           // The stream will have 2 channels 
uint8_t to_channels = 2;                                             // The stream will have 2 channels 
SineWaveGenerator<int16_t> sine_wave(32000);                         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(sine_wave);                  // Stream generated from sine wave
CsvStream<int16_t> out(Serial, to_channels);                         // Output to Serial
ChannelFormatConverterStream conv(out);
StreamCopy copier(conv, in_stream);                                  // copies sound to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  sine_wave.begin(from_channels, sample_rate, N_B4);
  conv.begin(from_channels, to_channels);
  in_stream.begin();

  out.begin();
}

void loop(){
    copier.copy();
}
