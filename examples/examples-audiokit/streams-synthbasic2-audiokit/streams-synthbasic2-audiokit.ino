/**
 * @file streams-synthbasic-audiokit.ino
 * @author Phil Schatzmann
 * @brief see https://www.pschatzmann.ch/home/2021/12/17/ai-thinker-audio-kit-building-a-simple-synthesizer-with-the-audiotools-library/)
 *        ADSR
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"

AudioBoardStream kit(AudioKitEs8388V1);
SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> sine_stream(sine); 
AudioEffectStream effects(sine_stream);
ADSRGain adsr(0.0001,0.0001, 0.9 , 0.0002);
StreamCopy copier(kit, effects); 

void actionKeyOn(bool active, int pin, void* ptr){
  Serial.println("KeyOn");
  float freq = *((float*)ptr);
  sine.setFrequency(freq);
  adsr.keyOn();
}

void actionKeyOff(bool active, int pin, void* ptr){
  Serial.println("KeyOff");
  adsr.keyOff();
}

// We want to play some notes on the AudioKit keys 
void setupActions(){
  // assign buttons to notes
  auto act_low = AudioActions::ActiveLow;
  static float note[] = {N_C3, N_D3, N_E3, N_F3, N_G3, N_A3}; // frequencies
  kit.audioActions().add(kit.getKey(1), actionKeyOn, actionKeyOff, act_low, &(note[0])); // C3
  kit.audioActions().add(kit.getKey(2), actionKeyOn, actionKeyOff, act_low, &(note[1])); // D3
  kit.audioActions().add(kit.getKey(3), actionKeyOn, actionKeyOff, act_low, &(note[2])); // E3
  kit.audioActions().add(kit.getKey(4), actionKeyOn, actionKeyOff, act_low, &(note[3])); // F3
  kit.audioActions().add(kit.getKey(5), actionKeyOn, actionKeyOff, act_low, &(note[4])); // G3
  kit.audioActions().add(kit.getKey(6), actionKeyOn, actionKeyOff, act_low, &(note[5])); // A3
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // setup effects
  effects.addEffect(adsr);

  // Setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);
  kit.setVolume(80);

  // Setup sound generation based on AudioKit settins
  sine.begin(cfg, 0);
  sine_stream.begin(cfg);
  effects.begin(cfg);

  // activate keys
  setupActions();
 

}

// copy the data
void loop() {
  copier.copy();
  kit.processActions();
}
