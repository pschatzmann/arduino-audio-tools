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

int channels = 2;
AudioKitStream kit;
Synthesizer synthesizer;
GeneratedSoundStream<int16_t> in(synthesizer); 
SynthesizerKey keys[] = {{PIN_KEY1, N_C3},{PIN_KEY2, N_D3},{PIN_KEY3, N_E3},{PIN_KEY4, N_F3},{PIN_KEY5, N_G3},{PIN_KEY6, N_A3},{0,0}};

int32_t get_sound_data(Frame *data, int32_t frameCount) {
  int frame_size = sizeof(int16_t)*channels;
  int16_t samples = synthesizer.readBytes((uint8_t*)data,frameCount*frame_size);
  //esp_task_wdt_reset();
  delay(1);
  return samples/frame_size;
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
  cfg.channels = channels;
  cfg.sample_rate = 44100;
  cfg.buffer_size = 512;
  cfg.buffer_count = 10;
  in.begin(cfg);

  a2dp_source.start("LEXON MINO L", get_sound_data);  
  a2dp_source.set_volume(20); 
}

void loop() {
  kit.processActions();
  delay(1);
}