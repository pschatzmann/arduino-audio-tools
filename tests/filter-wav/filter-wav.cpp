// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"
#include "SdFat.h"

// define FIR filter
float coef[] = { 0.021, 0.096, 0.146, 0.096, 0.021};
// 
uint16_t sample_rate=44100;
uint8_t channels = 2;                                             // The stream will have 2 channels 
NoiseGenerator<int16_t> noise(32000);                             // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(noise);                   // Stream generated from sine wave
FilteredStream<int16_t, float> in_filtered(in_stream, channels);  // Defiles the filter as BaseConverter
SdFat sd;
SdFile file;
EncodedAudioStream out(&file, new WAVEncoder());                  // encode as wav file
StreamCopy copier(out, in_stream);                                // copies sound to out

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  auto cfg = noise.defaultConfig();
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = 16;
  noise.begin(cfg);
  in_stream.begin();
  out.begin();

  // setup filters on channel 1 only
  in_filtered.setFilter(1, new FIR<float>(coef));
  
  if (!sd.begin(SS, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!file.open("noise.wav", O_RDWR | O_CREAT)) {
    sd.errorHalt("opening noise.wav for write failed");
  }

  for (int j=0;j<1024;j++){
    copier.copy();
  }
  file.close();
  stop();
}

void loop(){
}

