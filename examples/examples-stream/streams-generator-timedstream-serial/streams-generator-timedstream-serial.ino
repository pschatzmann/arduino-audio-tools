
/**
 * @file streams-generator-serial.ino
 * @author Phil Schatzmann
 * @brief Example how to limit the output with a timed stream: generate audio for 1 second
 * @copyright GPLv3
 **/

#include "AudioTools.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
TimedStream timed(sound, 0, 1);
CsvOutput<int16_t> out(Serial);
StreamCopy copier(out, timed);  // copies sound to out

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  sineWave.begin(info, N_B4);

  timed.begin(info);

  out.begin(info);
}

// Arduino loop - copy sound to out
void loop() {
  copier.copy();
}