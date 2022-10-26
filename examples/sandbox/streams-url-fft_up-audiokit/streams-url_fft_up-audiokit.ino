/**
 * @file streams-url_fft_up-audiokit.ino
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
#include "AudioCodecs/CodecMP3Helix.h"
#include "AudioLibs/AudioRealFFT.h" // or AudioKissFFT

URLStream url("SSID","PWD");
AudioKitStream kit;  
AudioRealFFT fft; 
ChannelFormatConverterStream cf(fft);
EncodedAudioStream dec(&cf, new MP3DecoderHelix()); // Decoding stream
StreamCopy copier(dec, cf);  // copy mic to tfl
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
  int n = 0;
  //fft.shiftValues(real, n);
  //fft.shiftValues(img, n);
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
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup Audiokit
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.channels = channels;
  cfg.sample_rate = samples_per_second;
  cfg.bits_per_sample = bits_per_sample;
  kit.begin(cfg);
  kit.setVolume(1.0);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 1024;
  tcfg.channels = channels;
  tcfg.sample_rate = samples_per_second;
  tcfg.bits_per_sample = bits_per_sample;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);

  copier.setCheckAvailableForWrite(false);
  // mp3 radio
  url.begin("https://stream.live.vc.bbcmedia.co.uk/bbc_world_service","audio/mp3");

  // convert 2 to 1 channel
  cf.begin(2,1);

  // initialize decoder
  dec.begin();

}

void loop() { 
  copier.copy();
}