/**
 * @file streams-sine-audiokit.ino
 * @brief We just use the sine generator from STK
 * @author Phil Schatzmann
 * @copyright Copyright (c) 2021
 */

#include "SD_MMC.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioSTK.h" // install https://github.com/pschatzmann/Arduino-STK
#include "AudioTools/AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver

SineWave sine;
STKStream<SineWave> in(sine);
AudioBoardStream out(AudioKitEs8388V1);
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
      freq = notes_array[random(sizeof(notes_array)/sizeof(float))];

      Serial.print("playing ");
      Serial.println(freq);

      sine.setFrequency(freq);
      timeout = millis()+800;
      active = false;
    } else {
      // silence for 100 ms
      sine.setFrequency(0);
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