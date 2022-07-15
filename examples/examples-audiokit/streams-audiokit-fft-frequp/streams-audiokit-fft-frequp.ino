/**
 * @file streams-audiokit-fft-frequp.ino
 * @author Phil Schatzmann
 * @brief Using FFT to shift frequency up quite a bit
 * @version 0.1
 * @date 2022-06-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioRealFFT.h" // or AudioKissFFT

AudioKitStream kit;  // Audio source
AudioRealFFT fft; 
StreamCopy copier(fft, kit);  // copy mic to tfl
int channels = 2;
int samples_per_second = 44100;
int bits_per_sample = 16;
float value=0;

// process fft result
void fftResult(AudioFFTBase &bfft){
  // get result components
  float* real = fft.realArray();
  float* img = fft.imgArray();
  // increase frequencies by n bins
  int n = 400;
  fft.shiftValues(real, n);
  fft.shiftValues(img, n);
  // invert fft with updated values
  float *result = fft.ifft(real, img);

  // output audio
  int16_t samples[2];
  for (int j=0;j<fft.length();j++){
    //Serial.println(result[j]);
    samples[0] = result[j]; // channel 0
    samples[1] = result[j]; // channel 1
    kit.write((uint8_t*)&samples, sizeof(samples));
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup Audiokit
  auto cfg = kit.defaultConfig(RXTX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  cfg.bits_per_sample = bits_per_sample;
  kit.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 4096;
  tcfg.channels = channels;
  tcfg.sample_rate = samples_per_second;
  tcfg.bits_per_sample = bits_per_sample;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);
}

void loop() { 
  copier.copy();
}