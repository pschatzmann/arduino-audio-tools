#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "StkAll.h"
#include "Midi.h"
#include "ArdStkMidiAction.h" // needed if midi is deactivated in ArdConfig.h

AudioKitStream kit;
Clarinet clarinet(440);
StkMidiAction action;
MidiBleServer ble("MidiServer", &action);
ArdStreamOut output(&kit);
int midiChannel = 0;

void actionKeyOn(bool active, int pin, void* ptr){
  int noteNumber = *((int*)ptr);
  Serial.print(noteNumber);
  Serial.println(" on");
  action.onNoteOn(midiChannel, noteNumber);
}

void actionKeyOff(bool active, int pin, void* ptr){
  int noteNumber = *((int*)ptr);
  Serial.print(noteNumber);
  Serial.println(" off");
  action.onNoteOff(midiChannel, noteNumber);
}

// We want to play some notes on the AudioKit keys 
void setupActions(){
  // assign buttons to notes
  auto act_low = AudioActions::ActiveLow;
  static int note[] = {48,50,52,53,55,57};
  kit.audioActions().add(PIN_KEY1, actionKeyOn, actionKeyOff, act_low, &(note[0])); // C3
  kit.audioActions().add(PIN_KEY2, actionKeyOn, actionKeyOff, act_low, &(note[1])); // D3
  kit.audioActions().add(PIN_KEY3, actionKeyOn, actionKeyOff, act_low, &(note[2])); // E3
  kit.audioActions().add(PIN_KEY4, actionKeyOn, actionKeyOff, act_low, &(note[3])); // F3
  kit.audioActions().add(PIN_KEY5, actionKeyOn, actionKeyOff, act_low, &(note[4])); // G3
  kit.audioActions().add(PIN_KEY6, actionKeyOn, actionKeyOff, act_low, &(note[5])); // A3
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  // define data format
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.channels = 1;
  cfg.bits_per_sample = 16;
  cfg.sample_rate = Stk::sampleRate();
  kit.begin(cfg);

  // play notes with keys
  setupActions();
 
  action.addInstrument(&clarinet, 1);
  ble.start();
}

void loop() {
  output.tick( action.tick() );
  kit.processActions();

}