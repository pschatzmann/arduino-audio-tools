//
// Arduino Audio Tools - Audio Out Example for STM32F723E Discovery Board
// This example generates a sine wave and outputs it via SAI2 to the
// on-board WM8994 codec.
// The example uses the AudioBoardStream class which is a subclass of
// I2SCodecStream
//
// The WM8994 is connected via SAI2 (pins PI4/PI5/PI6/PI7), not the classic
// I2S peripheral used on the STM32F411 Discovery - this board has no
// STM_I2S_PINS entry in stm32-i2s, so AudioBoardStream/I2SStream drive it
// through I2SDriverSTM32SAI instead, which wraps the
// https://github.com/pschatzmann/stm32-sai library (requires its
// STM32DriverF723.h board config). Output-only for now; duplex/audio-in
// would need a second SD pin (PG10) added to that library's PinConfig.
//

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
// Required so Arduino's library auto-detection pulls in stm32-sai (it only
// reacts to a bare, unconditional #include like this one - the internal
// __has_include("STM32AudioSAI.h") check in I2SStream.h can't trigger it).
#include <STM32AudioSAI.h>

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
