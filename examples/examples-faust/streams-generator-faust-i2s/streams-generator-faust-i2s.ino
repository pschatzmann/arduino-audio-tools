/**
 * @file streams-faust-i2s.ino
 * @author Phil Schatzmann
 * @brief Example how to use Faust as "Singal Processor"
 * @version 0.1
 * @date 2022-04-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioFaust.h"
#include "volume.h"

SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
mydsp dsp;
AudioKitStream out; 
FaustStream<mydsp> faust(out);
StreamCopy copier(faust, sound);  // copy mic to tfl

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // Setup Faust volume control
  auto cfg = faust.defaultConfig();
  faust.begin(cfg);
  faust.setLabelValue("0x00", 0.5);

  // Setup sine wave generator
  sineWave.begin(cfg.channels, cfg.sample_rate, N_B4);

  // start I2S
  auto cfg_i2s = out.defaultConfig(TX_MODE);
  cfg_i2s.sample_rate = cfg.sample_rate; 
  cfg_i2s.channels = cfg.channels;
  cfg_i2s.bits_per_sample = cfg.bits_per_sample;
  out.begin(cfg_i2s);

}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
