/**
 * @file streams-synthbasic-audiokit.ino
 * @author Phil Schatzmann
 * @brief see https://www.pschatzmann.ch/home/2021/12/17/ai-thinker-audio-kit-building-a-simple-synthesizer-with-the-audiotools-library/)
 *        Midi Support
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include <Midi.h>

SineWaveGenerator<int16_t> sine;
GeneratedSoundStream<int16_t> sine_stream(sine); 
ADSRGain adsr(0.0001,0.0001, 0.9 , 0.0002);
AudioEffectStream effects(sine_stream);
AudioKitStream kit;
StreamCopy copier(kit, effects); 

class SynthAction : public MidiAction {
    public:
        void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
           int frq = MidiCommon::noteToFrequency(note);
           sine.setFrequency(frq);
           adsr.keyOn();
        }
        void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
           adsr.keyOff();
        }
        void onControlChange(uint8_t channel, uint8_t controller, uint8_t value) {}
        void onPitchBend(uint8_t channel, uint8_t value) {}
} action;

MidiBleServer ble("MidiServer", &action);


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
  kit.audioActions().add(PIN_KEY1, actionKeyOn, actionKeyOff, act_low, &(note[0])); // C3
  kit.audioActions().add(PIN_KEY2, actionKeyOn, actionKeyOff, act_low, &(note[1])); // D3
  kit.audioActions().add(PIN_KEY3, actionKeyOn, actionKeyOff, act_low, &(note[2])); // E3
  kit.audioActions().add(PIN_KEY4, actionKeyOn, actionKeyOff, act_low, &(note[3])); // F3
  kit.audioActions().add(PIN_KEY5, actionKeyOn, actionKeyOff, act_low, &(note[4])); // G3
  kit.audioActions().add(PIN_KEY6, actionKeyOn, actionKeyOff, act_low, &(note[5])); // A3
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