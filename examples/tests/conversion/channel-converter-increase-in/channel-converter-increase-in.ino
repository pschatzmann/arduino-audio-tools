#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
uint8_t to_channels = 2;                                             // The stream will have 2 channels 
SineWaveGenerator<int16_t> sine_wave(32000);                         // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(sine_wave);                  // Stream generated from sine wave
CsvOutput<int16_t> out(Serial, to_channels);                         // Output to Serial
ChannelFormatConverterStream conv(in_stream);
StreamCopy copier(out, conv);                                       // copies converted sound to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  sine_wave.begin(info, N_B4);
  conv.begin(info, to_channels);
  in_stream.begin();

  out.begin();
}

void loop(){
    copier.copy();
}
