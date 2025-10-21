#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);  // subclass of SoundGenerator with max amplitude of 32000
AudioBoardStream out(AudioKitEs8388V1);
RTTTLOutput<int16_t> rtttl(sineWave, out);

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  delay(5000);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  out.begin(config);

  // Setup sine wave (optional)
  sineWave.begin(info);
  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() {
  rtttl.begin(info);
  rtttl.print("ComplexDemo: d=8, o=5, b=140: c4 e g c6 a5 g4 e g a g4 e c4 e2 g4 c6 a5 g4 e g a g4 e c2 p c4 e g c6 a5 g4 e g a g4 e c4 e2 g4 c6 a5 g4 e g a g4 e c2");
  delay(1000);
}
