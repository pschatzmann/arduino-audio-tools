#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // using RealFFT

AudioInfo info(44100, 1, 16);
AudioRealFFT fft; // or AudioKissFFT
Hann hann;
BufferedWindow buffered(&hann);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> in(sineWave);
StreamCopy copier(fft, in);
CsvStream<int16_t> out(Serial);
StreamCopy copierOut(out, fft);
float value = 0;

// display fft result
void fftResult(AudioFFTBase &fft) {
  float diff;
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
    Serial.print(fft.resultTime() - fft.resultTimeBegin());
    Serial.println();
  }
  // execute ifft
  copierOut.copyAll();
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // set the frequency
  sineWave.setFrequency(N_B4);

  // Setup sine wave
  auto cfg = in.defaultConfig();
  cfg.copyFrom(info);
  in.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig(RXTX_MODE);
  tcfg.window_function = &buffered;
  fft.begin(tcfg);

  // setup output
  out.begin(info);
}

void loop() { copier.copy(); }