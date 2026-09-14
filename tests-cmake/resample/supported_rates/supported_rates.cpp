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

  // readBytes(): when the source rate is already supported, bytes must
  // pass straight through unchanged (no resampling artifacts)
  int16_t samples[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  MemoryStream mem((const uint8_t *)samples, sizeof(samples));
  SupportedRatesStream in(mem);
  in.setSupportedSampleRates({8000, 16000, 22050, 32000, 44100, 48000});
  in.begin(AudioInfo(44100, 1, 16));

  int16_t result[8] = {0};
  size_t read = in.readBytes((uint8_t *)result, sizeof(result));
  assert(read == sizeof(result));
  for (int i = 0; i < 8; i++) assert(result[i] == samples[i]);

  // readBytes(): when the source rate is NOT supported, data must still
  // flow correctly through the resampler (upsampled -> more output bytes
  // than input bytes for the same source data)
  mem.rewind();
  SupportedRatesStream in2(mem);
  in2.setSupportedSampleRates({48000});
  in2.begin(AudioInfo(8000, 1, 16));
  assert(in2.audioInfoOut().sample_rate == 48000);

  uint8_t resampled[64] = {0};
  size_t total = 0;
  for (int i = 0; i < 10 && total < sizeof(resampled); i++) {
    size_t r = in2.readBytes(resampled + total, sizeof(resampled) - total);
    if (r == 0) break;
    total += r;
  }
  assert(total > sizeof(samples));
}

void loop() {}

int main() {
  setup();
  printf("OK\n");
  return 0;
}
