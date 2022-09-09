/**
 * @file streams-stk_loop-audiokit.ino
 * @author Phil Schatzmann
 * @brief We can use a MemoryLoop as input
 * @version 0.1
 * @date 2022-09-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioSTK.h"

MemoryLoop mloop("sinewave.raw", sinewave_raw, sinewave_raw_len);
STKStream<MemoryLoop> in(mloop);
AudioKitStream out;
StreamCopy copier(out, in);
MusicalNotes notes;

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
      freq = notes_array[random(sizeof(notes_array)/sizeof(uint16_t))];
      mloop.setFrequency(freq);
      timeout = millis()+800;
      active = false;
    } else {
      // silence for 100 ms
      mloop.setFrequency(0);
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
  ocfg.sd_active = false;
  out.begin(ocfg);

}

void loop() {
  play();
  copier.copy();
}
