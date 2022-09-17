/**
 * @file streams-stk_allinstruments-audioout.ino
 * @brief Test sketch which pays all notes of all instruments
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */
#include "AudioTools.h"
#include "AudioLibs/AudioSTK.h"

#ifdef IS_DESKTOP
#include "AudioLibs/PortAudioStream.h"
typedef PortAudioStream MyStdOut;
#else
#include "AudioLibs/AudioKit.h"
typedef AudioKitStream MyStdOut;

#endif
Instrmnt *p_Instrmnt=nullptr; // will be allocated dynamically
STKStream<Instrmnt> in;
MyStdOut out; // or replace with I2SStream or any other output class
StreamCopy copier(out, in);
MusicalNotes notes;

/// We do not have enough memory to allocate all instruments, so we provide an array with allocator methods
struct InstrumentInfo {
    const char* name;
    std::function<Instrmnt * ()> instrument;
};

InstrumentInfo instrumentArray[] = {
    {"BeeThree", []() { return new BeeThree(); }},
    {"BlowHole", []() { return new BlowHole(20); }},
    {"Bowed", []() { return new Bowed(); }},
    {"Clarinet", []() { return new Clarinet(); }},
//    {"Drummer", []() { return new Drummer(); }},  // comment out for STM32 or ESP8266
    {"Flute", []() { return new Flute(20); }},
    {"Rhodey", []() { return new Rhodey(); }},
    {"TubeBell", []() { return new TubeBell(); }},
    {"Mandolin", []() { return new Mandolin(20); }},
    {"ModalBar", []() { return new ModalBar(); }},
    {"Moog", []() { return new Moog(); }},
    {"Plucked", []() { return new Plucked(); }},
    {"Saxofony", []() { return new Saxofony(20); }},
    {"Shakers", []() { return new Shakers(); }},
    {"Sitar", []() { return new Sitar(); }},
    {"StifKarp", []() { return new StifKarp(); }},
    {"TubeBell", []() { return new TubeBell(); }},
    {"Wurley", []() { return new Wurley(); }},
    {"BlowBotl", []() { return new BlowBotl(); }},
    {"Brass", []() { return new Brass(); }},
    {"FMVoices", []() { return new FMVoices(); }},
    {"PercFlut", []() { return new PercFlut(); }},
    {"HevyMetl", []() { return new HevyMetl(); }},
    {"Recorder", []() { return new Recorder(); }},
    {"Resonate", []() { return new Resonate(); }},
    {"Simple", []() { return new Simple(); }},
    {"Whistle", []() { return new Whistle(); }},
    {nullptr, nullptr}
};
bool active = true;
uint64_t timeout = 0;
int instrumentIdx = 0;
int noteMin = 0;
int noteMax = 127;
int noteStep = 12; // 1 octave
int noteIndex = noteMin;
int onLengthMs = 400;
int offLengthMs = 50;
float amplitude = 1.0;

void selectInstrument() {
  if (p_Instrmnt==nullptr){
    Serial.println(instrumentArray[instrumentIdx].name);
    p_Instrmnt = instrumentArray[instrumentIdx].instrument();
    in.setInput(p_Instrmnt);
    in.begin();
  }
}

void noteOn() {
  // play note for 800 ms
  if (p_Instrmnt!=nullptr){
    float freq = notes.stkNoteToFrequency(noteIndex);
    p_Instrmnt->noteOn(freq, amplitude);
  }
  timeout = millis()+onLengthMs;
  active = false;
}

void noteOff() {
  // note off - silence for 100 ms
  if (p_Instrmnt!=nullptr){
    p_Instrmnt->noteOff(amplitude);
  }
  timeout = millis()+offLengthMs;

  // select next note
  noteIndex += noteStep;
  // switch instrument after we played all notes
  if (noteIndex>=noteMax){
    noteIndex = noteMin;
    instrumentIdx++;
    if (instrumentArray[instrumentIdx].name==nullptr){
      instrumentIdx = 0;
    }
    // delete old instrument
    if (p_Instrmnt!=nullptr){
      delete p_Instrmnt;
      p_Instrmnt = nullptr; 
      selectInstrument();
    }
  }
  active = true;
}

void play() {

  if (millis()>timeout){
    if (active){
      Serial.print("- playing ");
      Serial.println(noteIndex);
      noteOn();
    } else {
      noteOff();
    }
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  StkLogLevel = StkWarning;

  // setup input
  auto icfg = in.defaultConfig();
  in.begin(icfg);

  // setup output
  auto ocfg = out.defaultConfig(TX_MODE);
  ocfg.copyFrom(icfg);
//  ocfg.sd_active = false;
  out.begin(ocfg);

  // select first instument
  selectInstrument();

}

void loop() {
  play();
  copier.copy();
}
