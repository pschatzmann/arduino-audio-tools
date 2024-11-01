#include "AudioTools.h"
//#include "fir_coeffs_61Taps_44100_200_19000.h"
//#include "fir_coeffs_101Taps_44100_200_19000.h"
#include "fir_coeffs_161Taps_44100_200_19000.h"

AudioInfo from(44100, 2, 32);
AudioInfo to(44100, 2, 16);
SineWaveGenerator<int32_t> sineWave;            
GeneratedSoundStream<int32_t> in(sineWave);           
CsvOutput<int16_t> out;
FilteredStream<int16_t, float> filtered(out, from.channels); 
NumberFormatConverterStream conv (filtered);
StreamCopy copier(conv, in);


// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
  filtered.setFilter(0, new FIR<float>(coeffs_hilbert_161Taps_44100_200_19000));
  filtered.setFilter(1, new FIR<float>(coeffs_delay_161));

  // start in
  auto config_in = in.defaultConfig();
  config_in.copyFrom(from);
  in.begin(config_in);
  sineWave.begin(config_in, 8000.0);

  // start out
  auto config_out = out.defaultConfig(TX_MODE);
  config_out.copyFrom(to);
  out.begin(config_out);

  // Start conversion
  conv.begin(from, to);
}

// Arduino loop - copy sound to out
void loop() {
  copier.copy();
}
