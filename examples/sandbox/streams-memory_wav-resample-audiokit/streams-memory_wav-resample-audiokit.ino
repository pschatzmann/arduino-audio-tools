#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "g8-sine.h"
#include "g8-sine-long.h"
#include "g8-saw.h"

// sawtooth note g8 recorded at 96000
MemoryStream toneG8(g8_sine_raw, g8_sine_raw_len);
// playback at 24000 (4 times slower)
const AudioInfo info(24000, 1, 16);
AudioBoardStream out(AudioKitEs8388V1);
//CsvOutput out(Serial);
//FilteredStream<int16_t, int16_t> filter(out, 1);  
ResampleStream resample(out); // replace with out
StreamCopy copier(resample, toneG8, 2048);  // copies sound to out
int idx_max = 100;
int idx = idx_max;
MusicalNotes notes;
uint32_t timeout = 0;

void changeNote() {
  const float from_tone = N_G8;
  // e.g. 6271.93f / 16.35f * 4.0 = 0.01042741229
  float step_size =  notes.frequency(idx) / from_tone * 4.0f;
  Serial.print("playing note: ");  
  Serial.print(notes.noteAt(idx));
  Serial.print(" / step: ");
  Serial.println(step_size);  

  resample.setStepSize(step_size);
  idx--;
  if (idx < 0) {
    idx = idx_max;
    Serial.println("-----------------------");
  }
  timeout = millis() + 1000;
}

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // filter.setFilter(0, new MedianFilter<int16_t>(7));
  // filter.begin(info);

  // open input
  toneG8.begin();
  toneG8.setLoop(true);

  // open resample
  auto rcfg = resample.defaultConfig();
  rcfg.copyFrom(info);
  resample.begin(rcfg);
  //resample.setBuffered(false);

  // open out output
  auto cfg = out.defaultConfig();
  cfg.copyFrom(info);
  out.begin(cfg);
  out.setVolume(0.3);
}

// Arduino loop - copy sound to out
void loop() {
  if (millis() > timeout) {
    changeNote();
  }
  copier.copy();
}