/**
 * @file streams-synth-audiokit.ino
 * @author Phil Schatzmann
 * @copyright GPLv3
 *  
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream kit;
Synthesizer synthesizer;
GeneratedSoundStream<int16_t> in(synthesizer); 
StreamCopy copier(kit, in); 
SynthesizerKey keys[] = {{PIN_KEY1, N_C3},{PIN_KEY2, N_D3},{PIN_KEY3, N_E3},{PIN_KEY4, N_F3},{PIN_KEY5, N_G3},{PIN_KEY6, N_A3},{0,0}};

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // Setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);
  kit.setVolume(80);

  // define synthesizer keys for AudioKit
  synthesizer.setKeys(kit.audioActions(), keys, AudioActions::ActiveLow);
  synthesizer.setMidiName("AudioKit Synthesizer");
  // Setup sound generation & synthesizer based on AudioKit default settings
  in.begin(cfg);

}

void loop() {
  copier.copy();
  kit.processActions();
}