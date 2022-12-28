
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioRealFFT.h" // or AudioKissFFT

AudioKitStream kit;  // Audio source
AudioRealFFT fft; // or AudioKissFFT
StreamCopy copier(fft, kit);  // copy mic to tfl
int channels = 2;
int samples_per_second = 44100;
int bits_per_sample = 16;
float value=0;

// display fft result
void fftResult(AudioFFTBase &fft){
    float diff;
    auto result = fft.result();
    if (result.magnitude>100){
        Serial.print(result.frequency);
        Serial.print(" ");
        Serial.print(result.magnitude);  
        Serial.print(" => ");
        Serial.print(result.frequencyAsNote(diff));
        Serial.print( " diff: ");
        Serial.println(diff);
    }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup Audiokit
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  cfg.bits_per_sample = bits_per_sample;
  kit.begin(cfg);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 8192;
  tcfg.channels = channels;
  tcfg.sample_rate = samples_per_second;
  tcfg.bits_per_sample = bits_per_sample;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);
}

void loop() { 
  copier.copy();
}