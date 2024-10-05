/**
 * @file streams-stk-desktop.ino
 * @brief Build and run on desktop using PortAudio
 * build with 
 * - mkdir build
 * - cd build
 * - cmake -DCMAKE_BUILD_TYPE=Debug ..
 * - make
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioSTK.h" // install https://github.com/pschatzmann/Arduino-STK3
#include "AudioTools/AudioLibs/PortAudioStream.h"

Flute insturment(50);
STKStream<Instrmnt> in(insturment);
PortAudioStream out;
StreamCopy copier(out, in);
MusicalNotes notes;

float note_amplitude = 0.5;
static float notes_array[] = { // frequencies aleatoric C-scale
    N_C3, N_D3, N_E3, N_F3, N_G3, N_A3, N_B3,
    N_C4, N_D4, N_E4, N_F4, N_G4, N_A4, N_B4,
    N_C5, N_D5, N_E5, N_F5, N_G5, N_A5, N_B5
};

void play() {
  static bool active=true;
  static float freq;
  static uint64_t timeout;

  if (millis()>timeout){
    if (active){
      // play note for 800 ms
      freq = notes_array[random(static_cast<long>(sizeof(notes_array)/sizeof(float)))];

      Serial.print("playing ");
      Serial.println(freq);

      insturment.noteOn(freq, note_amplitude);
      timeout = millis()+800;
      active = false;
    } else {
      // silence for 100 ms
      insturment.noteOff(note_amplitude);
      timeout = millis()+100;
      active = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // setup input
  auto icfg = in.defaultConfig();
  in.begin(icfg);

  // setup output
  auto ocfg = out.defaultConfig(TX_MODE);
  ocfg.copyFrom(icfg);
  out.begin(ocfg);

}

void loop() {
  play();
  copier.copy();
}