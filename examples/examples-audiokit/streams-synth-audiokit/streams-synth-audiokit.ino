/**
 * @file streams-synth-audiokit.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 *  
 */
#define USE_MIDI

#include "AudioTools.h"
#include "AudioTools/Synthesizer.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream kit;
Synthesizer synthesizer;
SynthesizerKey keys[] = {{PIN_KEY1, N_C3},{PIN_KEY2, N_D3},{PIN_KEY3, N_E3},{PIN_KEY4, N_F3},{PIN_KEY5, N_G3},{PIN_KEY6, N_A3},{0,0}};
GeneratedSoundStream<int16_t> in(synthesizer); 
StreamCopy copier(kit, in); 

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Info);

  // Setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  kit.begin(cfg);

  // define synthesizer keys for AudioKit
  synthesizer.setKeys(kit.audioActions(), keys, AudioActions::ActiveLow);
  synthesizer.setMidiName("AudioKit Synthesizer");

  // Setup sound generation based on AudioKit settings
  in.begin(cfg);

}

void loop() {
  copier.copy();
  kit.processActions();
}