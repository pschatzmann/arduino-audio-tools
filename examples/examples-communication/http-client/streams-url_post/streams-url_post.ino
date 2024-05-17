
/**
 * @file streams-url-post.ino
 * @author Phil Schatzmann
 * @brief example how to http post data from an input stream
 * @copyright GPLv3
 */

#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
TimedStream timed(sound);
URLStream url("ssid", "password");

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // limit the size of the input stream
  timed.setEndSec(60);
  timed.begin();

  // post the data
  url.begin("http://192.168.1.35:8000", "audio/bin", POST, "text/html", timed);
}

// Arduino loop - copy sound to out
void loop() {}
