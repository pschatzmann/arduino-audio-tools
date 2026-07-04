//
// Arduino Audio Tools - Audio Out Example for STM32F411 Discovery Board
// This example generates a sine wave and outputs it via I2S to the on-board DAC
// The example uses the AudioBoardStream class which is a subclass of
// I2SCodecStream
//

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
AudioBoardStream out(STM32F411Disco);
StreamCopy copier(out, sound);  // copies sound into i2s

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  while(!Serial) delay(100);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  if (!out.begin(config)) {
    Serial.println("error!");
    stop();
  }

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }
