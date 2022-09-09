/**
 * @file streams-stk-desktop.ino
 * @brief Build and run on desktop using PortAudio
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/AudioSTK.h"
#include "AudioLibs/PortAudioStream.h"

STKStream<Instrmnt> in;
PortAudioStream out;
StreamCopy copier;
MusicalNotes notes;
Instrmnt* p_instrument=nullptr; // instrument depends on file system

float note_amplitude = 0.5;
static uint16_t notes_array[] = { // frequencies aleatoric C-scale
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
      freq = notes_array[random(static_cast<long>(sizeof(notes_array)/sizeof(uint16_t)))];

      Serial.print("playing ");
      Serial.println(freq);

      p_instrument->noteOn(freq, note_amplitude);
      timeout = millis()+800;
      active = false;
    } else {
      // silence for 100 ms
      p_instrument->noteOff(note_amplitude);
      timeout = millis()+100;
      active = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial,AudioLogger::Warning);

  // We can allocate the incstument only after SD_M
  p_instrument = new BeeThree();
  in.setInput(p_instrument);;

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