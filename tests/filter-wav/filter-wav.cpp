// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/PortAudioStream.h"
#include "SdFat.h"

// define FIR filter
int16_t coef[] = { 
-600,
-1859,
608,
3200,
-612,
-9752,
18032,
-9752,
-612,
3200,
608,
-1859,
-600
};
// 
uint16_t sample_rate=44100;
uint8_t channels = 2;                                             // The stream will have 2 channels 
SineWaveGenerator<int16_t> wave(32000);                             // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in_stream(wave);                   // Stream generated from sine wave
FilteredStream<int16_t, int16_t> in_filtered(in_stream, channels);  // Defiles the filter as BaseConverter
SdFat sd;
SdFile file;
EncodedAudioStream out(&file, new WAVEncoder());                  // encode as wav file
StreamCopy copier(out, in_filtered);                                // copies sound to out
MusicalNotes notes;

void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  auto cfg = wave.defaultConfig();
  cfg.sample_rate = sample_rate;
  cfg.channels = channels;
  cfg.bits_per_sample = 16;
  wave.begin(cfg, 20);
  in_stream.begin();
  out.begin();

  // setup filters on channel 1 only
  in_filtered.setFilter(1, new FIR<int16_t>(coef));
  
  if (!sd.begin(SS, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!file.open("wave.wav", O_RDWR | O_CREAT)) {
    sd.errorHalt("opening wave.wav for write failed");
  }

  for (int j=0;j<notes.frequencyCount();j++){
      wave.setFrequency(notes.frequency(j));
      for (int i=0;i<20;i++){
        copier.copy();
      }
  }
  file.close();
  stop();
}

void loop(){
}

