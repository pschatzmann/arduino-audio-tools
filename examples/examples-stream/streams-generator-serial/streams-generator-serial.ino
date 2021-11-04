/**
 * @file streams-url_raw-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-url_raw-serial/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"

using namespace audio_tools;  

typedef int16_t sound_t;                                   // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> sound(sineWave);             // Stream generated from sine wave
CsvStream<sound_t> printer(Serial, channels);              // ASCII stream 
StreamCopy copier(printer, sound);                         // copies sound into printer

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
}


// Arduino loop - repeated processing 
void loop() {
  copier.copy();
}