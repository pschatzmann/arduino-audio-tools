#include <cassert>
#include <cstdio>

#include "AudioTools.h"

using namespace audio_tools;

AudioInfo info(44100, 1, 16);
CsvOutput<int16_t> csv(Serial);
SupportedRatesStream out(csv);

void setup(void) {
  out.setSupportedSampleRates({8000, 16000, 22050, 32000, 48000});
  out.begin(info);
  assert(out.audioInfoOut().sample_rate == 48000);

  // switch to a rate closest to 22050
  out.setAudioInfo(AudioInfo(20000, 1, 16));
  assert(out.audioInfoOut().sample_rate == 22050);

  // the corrected AudioInfo must have propagated to the downstream output
  assert(csv.audioInfo().sample_rate == 22050);
  assert(csv.audioInfo().channels == 1);
  assert(csv.audioInfo().bits_per_sample == 16);
}

void loop() {}

int main() {
  setup();
  printf("OK\n");
  return 0;
}
