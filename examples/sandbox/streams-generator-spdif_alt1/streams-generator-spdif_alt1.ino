/**
 * @file streams-generator-i2s.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-i2s/README.md 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioLibs/AudioESP8266.h"
#include "AudioOutputSPDIF.h"

using namespace audio_tools;  

typedef int16_t sound_t;                                   // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                      // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> sound(sineWave);             // Stream generated from sine wave
AudioOutputSPDIF spdif;                                    // ESP8288-Audio SPDIF Output
ESP3288AudioOutput out(spdif, channels);                             // Stream Adapter
StreamCopy copier(out, sound, 32);                         // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start I2S
  Serial.println("starting SPDIF...");
  // setup output
  spdif.SetPinout(-1, -1, 22);
  spdif.SetBitsPerSample(16); 
  spdif.SetChannels(channels); 
  spdif.SetRate(sample_rate);
  spdif.begin();

  // Setup sine wave
  sineWave.begin(channels, sample_rate, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
