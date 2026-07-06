//
// Arduino Audio Tools - Audio Out Example for STM32F723E Discovery Board
// This example generates a sine wave and outputs it via SAI2 to the
// on-board WM8994 codec.
// The example uses the AudioBoardStream class which is a subclass of
// I2SCodecStream
//
// NOTE: this is experimental. The WM8994 is connected via SAI2 (pins
// PI4/PI5/PI6/PI7), not the classic I2S peripheral used on the STM32F411
// Discovery. Getting audio flowing additionally requires:
//  - SAI2 pin/peripheral support in the "stm32-i2s" backend
//    (https://github.com/pschatzmann/stm32f411-i2s) used by I2SDriverSTM32
//  - matching entries in this board's PeripheralPins.c
// Treat this sketch as a template until that lower-level support lands.
//

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave;
GeneratedSoundStream<int16_t> sound(sineWave);
AudioBoardStream out(STM32F723Disco);
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
