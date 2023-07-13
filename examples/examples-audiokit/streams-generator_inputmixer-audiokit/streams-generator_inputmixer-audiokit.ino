/**
 * @file streams-generator_inputmixer-audiokit.ino
 * @brief Tesing input mixer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioInfo info(32000, 2, 16);
SineWaveGenerator<int16_t> sineWave1(32000);                // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t> sineWave2(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);             // Stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2);             // Stream generated from sine wave
InputMixer<int16_t> mixer;
AudioKitStream out; 
StreamCopy copier(out, mixer);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave1.begin(info, N_B4);
  sineWave2.begin(info, N_E4);

  mixer.add(sound1);
  mixer.add(sound2);
  mixer.begin(info);

  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
