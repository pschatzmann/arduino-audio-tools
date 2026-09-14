/**
 * @file streams-generator_inputmixer-audiokit.ino
 * @brief Tesing input mixer: On a ESP32 we can mix max 19
 * FastSineGenerator inputs together
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

constexpr int N = 19;
AudioInfo info(44100, 2, 16);
constexpr float amplitude = 32000.0f;
float notes[] = {N_B4, N_E4, N_G4, N_B5, N_E5, N_G5, N_B6, N_E6};
FastSineGenerator<int16_t> sineWaves[N];
GeneratedSoundStream<int16_t> sounds[N];
InputMixer<int16_t, int> mixer;
AudioBoardStream out(AudioKitEs8388V1);
StreamCopy copier(out, mixer);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // Setup sine waves
  for (int i = 0; i < N; i++) {
    sounds[i].setInput(sineWaves[i]);
    sineWaves[i].setAmplitude(amplitude);
    sineWaves[i].begin(info, notes[i % (sizeof(notes) / sizeof(notes[0]))]);
    mixer.add(sounds[i]);
  }
  mixer.begin(info);

  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }
