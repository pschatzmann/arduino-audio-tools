/**
 * @file basic-generator-a2dp.ino
 * @author Phil Schatzmann
 * @brief We send a test sine signal to a bluetooth speaker
 * @copyright GPLv3
*/

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"

const char* name = "LEXON MINO L";                        // Replace with your bluetooth speaker name  
SineWaveGenerator<int16_t> sineWave(15000);               // subclass of SoundGenerator, set max amplitude (=volume)
GeneratedSoundStream<int16_t> in_stream(sineWave);        // Stream generated from sine wave
BluetoothA2DPSource a2dp_source;                          // A2DP Sender

// callback used by A2DP to provide the sound data - usually len is 128 * 2 channel int16 frames
int32_t get_sound_data(uint8_t * data, int32_t len) {
  return in_stream.readBytes((uint8_t*)data, len);
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start input 
  auto cfg = in_stream.defaultConfig();
  cfg.bits_per_sample = 16;
  cfg.channels = 2;
  cfg.sample_rate = 44100;
  in_stream.begin(cfg);
  sineWave.begin(cfg, N_B4);


  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.set_auto_reconnect(false);
  a2dp_source.start_raw(name, get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
  delay(1000);
}