/**
 * @file streams-synth-audiokit.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 *  
 */

#define USE_MIDI
#include "AudioTools.h" // must be first
#include "AudioLibs/AudioBoardStream.h" // https://github.com/pschatzmann/arduino-audio-driver
#include "BluetoothA2DPSource.h" // https://github.com/pschatzmann/ESP32-A2DP

BluetoothA2DPSource a2dp_source;

int channels = 2;
AudioBoardStream kit(AudioKitEs8388V1);
Synthesizer synthesizer;
GeneratedSoundStream<int16_t> in(synthesizer); 
SynthesizerKey keys[] = {{kit.getKey(1), N_C3},{kit.getKey(2), N_D3},{kit.getKey(3), N_E3},{kit.getKey(4), N_F3},{kit.getKey(5), N_G3},{kit.getKey(6), N_A3},{0,0}};

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
  in.begin(cfg);

  a2dp_source.start("LEXON MINO L", get_sound_data);  
  a2dp_source.set_volume(20); 
}

void loop() {
  kit.processActions();
  delay(1);
}