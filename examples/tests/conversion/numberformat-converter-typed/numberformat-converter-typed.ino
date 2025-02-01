#include "AudioTools.h"

using target_t = int32_t; // uint8_t, int8_t, int16_t, uint16_t, int24_t, uint32_t, int32_t, FloatAudio
using source_t = int16_t; 
SineWaveGeneratorT<source_t> sineWave;                // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<source_t> sound(sineWave);             // Stream generated from sine wave
CsvOutputT<target_t> out(Serial, sound.audioInfo().channels);
NumberFormatConverterStreamT<source_t, target_t> nfc(out);
StreamCopy copier(nfc, sound);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  nfc.begin();
  out.begin();
  sineWave.begin();

  // Setup sine wave
  sineWave.setFrequency(N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
