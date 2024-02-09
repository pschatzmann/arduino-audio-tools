/**
 * Test case for MedianFilter
*/
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 1, 16);

// provide sine wave damaged with some rare random noise samples
class TestGenerator : public SoundGenerator<int16_t> {
public:
  TestGenerator() {
    gen_sine.begin(info);
    gen_sine.setFrequency(N_B4);
    gen_noise.begin(info);
  }
  virtual int16_t readSample() {
    // proper signal
    int16_t sample = gen_sine.readSample();
    if (count-- == 0){
      // plan for next random value
      count = random(100, 10000);
      // replace proper singal value with a random value
      sample = gen_noise.readSample();;
    }
    return sample;
  };
protected:
  SineWaveGenerator<int16_t> gen_sine;
  WhiteNoiseGenerator<int16_t> gen_noise;
  size_t count = 0;
} testSound;

GeneratedSoundStream<int16_t> testStream(testSound);             // Stream generated from sine wave
FilteredStream<int16_t, int16_t> inFiltered(testStream, info.channels);  // Defiles the filter as BaseConverter
MedianFilter<int16_t> medianFilter;
AudioBoardStream out(AudioKitEs8388V1);
StreamCopy copier(out, inFiltered);                             
//StreamCopy copier(out, testStream); //compare with this                            


void setup() {
  inFiltered.setFilter(0, &medianFilter);
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  out.begin(cfg);
}

void loop() {
  copier.copy();
}