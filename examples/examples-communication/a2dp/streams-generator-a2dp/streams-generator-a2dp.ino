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
#include "AudioLibs/A2DPStream.h"

const char* name = "LEXON MINO L";                         // Replace with your device name
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);               // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> in(sineWave);               // Stream generated from sine wave
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
  cfg.copyFrom(info);
  in.addNotifyAudioChange(out);
  in.begin(cfg);

  // We send the test signal via A2DP - so we conect to the MyMusic Bluetooth Speaker
  auto cfgA2DP = out.defaultConfig(TX_MODE);
  cfgA2DP.name = name;
  //cfgA2DP.auto_reconnect = false;
  out.begin(cfgA2DP);
  out.setVolume(0.3);
  Serial.println("A2DP is connected now...");

}

// Arduino loop  
void loop() {
  copier.copy();
}