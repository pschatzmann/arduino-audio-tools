/**
 * @file streams-generator_outputmixer-audiokit.ino
 * @brief Tesing output mixer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioInfo info(32000, 2, 16);
SineWaveGenerator<int16_t> sineWave1(32000);                // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t> sineWave2(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound1(sineWave1);            // Stream generated from sine wave
GeneratedSoundStream<int16_t> sound2(sineWave2);            // Stream generated from sine wave
AudioKitStream out; 
OutputMixer<int16_t> mixer(out, 2);                         // output mixer with 2 outputs mixing to AudioKitStream 
StreamCopy copier1(mixer, sound1);                          // copies sound into mixer
StreamCopy copier2(mixer, sound2);                          // copies sound into mixer

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info); 
  out.begin(config);

  // Setup sine wave
  sineWave1.begin(info, N_B4);
  sineWave2.begin(info, N_E4);

  // setup Output mixer with default buffer size
  mixer.begin();

  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  // write idx 0 to the mixer
  copier1.copy();
  // write idx 1 to the mixer and flush (because stream count = 2)
  copier2.copy();

  // We could flush to force the output but this is not necessary because we were already writing all streams
  //mixer.flushMixer();

}
