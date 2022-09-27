#include "AudioTools.h"
#include "AudioLibs/AudioKissFFT.h" // or AudioKissFFT

AudioKissFFT fft; // or AudioKissFFT
SineFromTable<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fft, in);
uint16_t sample_rate = 44100;
int bits_per_sample = 16;
int channels = 1;
float value = 0;
uint64_t timestamp;

// display fft result
void fftResult(AudioFFTBase &fft) {
  int diff;
  auto result = fft.result();
  if (result.magnitude > 100) {
    Serial.print(result.frequency);
    Serial.print(" ");
    Serial.print(result.magnitude);
    Serial.print(" => ");
    Serial.print(result.frequencyAsNote(diff));
    Serial.print(" diff: ");
    Serial.print(diff);
    Serial.print(" - time ms ");
    Serial.print(millis() - timestamp);
    Serial.println();
  }
  timestamp = millis();
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // set the frequency
  sineWave.setFrequency(N_B4);

  // Setup sine wave
  auto cfg = in.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  in.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.copyFrom(cfg);
  tcfg.length = 8192;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);
}

void loop() { copier.copy(); }