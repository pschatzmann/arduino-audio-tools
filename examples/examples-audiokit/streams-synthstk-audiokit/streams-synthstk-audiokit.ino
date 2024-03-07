/**
 * @file streams-synthstk-audiokit.ino
 * @brief see https://www.pschatzmann.ch/home/2021/12/17/ai-thinker-audiokit-a-simple-synthesizer-with-stk/
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "StkAll.h"

AudioBoardStream kit(AudioKitEs8388V1);
Clarinet clarinet(440);
Voicer voicer;
ArdStreamOut output(&kit);
float noteAmplitude = 128;
int group = 0;

void actionKeyOn(bool active, int pin, void* ptr){
  float note = *((float*)ptr);
  voicer.noteOn(note, noteAmplitude, group);
}

void actionKeyOff(bool active, int pin, void* ptr){
  float note = *((float*)ptr);
  voicer.noteOff(note, noteAmplitude, group);
}

// We want to play some notes on the AudioKit keys 
void setupActions(){
  // assign buttons to notes
  auto act_low = AudioActions::ActiveLow;
  static int note[] = {48,50,52,53,55,57}; // midi keys
  kit.audioActions().add(kit.getKey(1), actionKeyOn, actionKeyOff, act_low, &(note[0])); // C3
  kit.audioActions().add(kit.getKey(2), actionKeyOn, actionKeyOff, act_low, &(note[1])); // D3
  kit.audioActions().add(kit.getKey(3), actionKeyOn, actionKeyOff, act_low, &(note[2])); // E3
  kit.audioActions().add(kit.getKey(4), actionKeyOn, actionKeyOff, act_low, &(note[3])); // F3
  kit.audioActions().add(kit.getKey(5), actionKeyOn, actionKeyOff, act_low, &(note[4])); // G3
  kit.audioActions().add(kit.getKey(6), actionKeyOn, actionKeyOff, act_low, &(note[5])); // A3
}

void setup() {
  Serial.begin(9600);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  voicer.addInstrument(&clarinet, group);

  // define data format
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = Stk::sampleRate();
  cfg.sd_active = false;
  kit.begin(cfg);

  // play notes with keys
  setupActions();

}

void loop() {
   for (int j=0;j<1024;j++) {
      output.tick( voicer.tick() );
   }
   kit.processActions();
}
