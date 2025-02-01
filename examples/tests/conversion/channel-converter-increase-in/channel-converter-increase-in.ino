#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
AudioInfo to_info(44100, 2, 16);
SineWaveGeneratorT<int16_t> sine_wave(32000);                         // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> in_stream(sine_wave);                  // Stream generated from sine wave
CsvOutput out(Serial);                         // Output to Serial
ChannelFormatConverterStream conv(in_stream);
StreamCopy copier(out, conv);                                       // copies converted sound to out

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  sine_wave.begin(info, N_B4);
  conv.begin(info, to_info);
  in_stream.begin();

  out.begin(to_info);
}

void loop(){
    copier.copy();
}
