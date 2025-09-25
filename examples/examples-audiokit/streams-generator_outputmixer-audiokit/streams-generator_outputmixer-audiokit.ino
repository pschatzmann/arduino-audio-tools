/**
 * @file streams-generator_outputmixer-audiokit.ino
 * @brief Tesing output mixer
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
const int N = 5;
SineWaveGenerator<int16_t> sineWave[N](32000);
GeneratedSoundStream<int16_t> sound[N];
AudioBoardStream out(AudioKitEs8388V1);
OutputMixer<int16_t> mixer(out, N, DefaultAllocatorRAM);
StreamCopy copier[N](DefaultAllocatorRAM);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  for (int j = 0; j < N; j++) {
    sineWave[j].begin(info, 440 * (j + 1));
    sound[j].setInput(sineWave[j]);
    sound[j].begin(info);
    copier[j].begin(mixer, sound[j]);  
  }

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // setup Output mixer with default buffer size
  mixer.begin();

  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
  // write each output
  for (int j = 0; j < N; j++) {
    copier[j].copy();
  }

  // We could flush to force the output but this is not necessary because we
  // were already writing all streams
  // mixer.flushMixer();
}
