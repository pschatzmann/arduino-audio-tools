/**
 * @file streams-generator-a2dp.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-a2dp/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioA2DP.h"

typedef int16_t sound_t;                                  // sound will be represented as int16_t (with 2 bytes)
uint16_t sample_rate=44100;
uint8_t channels = 2;                                     // The stream will have 2 channels 
SineWaveGenerator<sound_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<sound_t> in(sineWave);               // Stream generated from sine wave
A2DPStream out;                                           // A2DP output
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // set the frequency
  sineWave.setFrequency(N_B4);

  // Setup sine wave
  auto cfg = in.defaultConfig();
  cfg.channels = channels;
  cfg.sample_rate = sample_rate;
  in.setNotifyAudioChange(out);
  in.begin(cfg);

  // We send the test signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  auto cfgA2DP = out.defaultConfig(TX_MODE);
  cfgA2DP.name = "LEXON MINO L";
  //cfgA2DP.auto_reconnect = false;
  out.begin(cfgA2DP);
  out.setVolume(0.3);
  Serial.println("A2DP is connected now...");

}

// Arduino loop  
void loop() {
  copier.copy();
}