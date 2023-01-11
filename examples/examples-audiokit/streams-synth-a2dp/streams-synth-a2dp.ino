/**
 * @file streams-synth-audiokit.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 *  
 */

#define USE_MIDI
#include "BluetoothA2DPSource.h"
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

BluetoothA2DPSource a2dp_source;

AudioKitStream kit;
Synthesizer synthesizer;
GeneratedSoundStream<int16_t> in(synthesizer); 
SynthesizerKey keys[] = {{PIN_KEY1, N_C3},{PIN_KEY2, N_D3},{PIN_KEY3, N_E3},{PIN_KEY4, N_F3},{PIN_KEY5, N_G3},{PIN_KEY6, N_A3},{0,0}};

int32_t get_sound_data(Frame *data, int32_t len) {
  int16_t sample = synthesizer.readSamples((int16_t(*)[2])data,len);
  //esp_task_wdt_reset();
  delay(1);
  return len;
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // setup synthezizer keys
  synthesizer.setKeys(kit.audioActions(), keys, AudioActions::ActiveLow);

  // define synthesizer keys for AudioKit
  synthesizer.setKeys(kit.audioActions(), keys, AudioActions::ActiveLow);
  synthesizer.setMidiName("AudioKit Synthesizer");
  auto cfg = in.defaultConfig();
  cfg.channels = 2;
  cfg.sample_rate = 44100;
  in.begin(cfg);

  a2dp_source.start("LEXON MINO L", get_sound_data);  
  a2dp_source.set_volume(20); 
}

void loop() {
  kit.processActions();
  delay(1);
}