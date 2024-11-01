/**
 * Test sketch for adc2dac.pd that was compiled with hvcc -n Adc2Dac adc2dac.pd
 * The ADC receives data from the SineWaveGenerator and the output from the
 * DAC is copied to I2S (AudioBoardStream)
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "Heavy_Adc2Dac.hpp"
#include "AudioTools/AudioLibs/PureDataStream.h"

AudioInfo info(44100, 2, 16);
Heavy_Adc2Dac pd_test(info.sample_rate);
PureDataStream pd(pd_test);
AudioBoardStream i2s(AudioKitEs8388V1);  // final output of decoded stream
SineWaveGenerator<int16_t> sineWave;     // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(sineWave);     // Stream generated from sine wave

StreamCopy copierToPd(pd, sound, 1024);
StreamCopy copierFromPd(i2s, pd, 1024);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup output
  auto config = i2s.defaultConfig(TX_MODE);
  config.sd_active = false;
  config.copyFrom(info);
  i2s.begin(config);

  // setup input
  sineWave.begin(info, N_B4);

  // setup pd
  pd.begin();

}

void loop() {
  // provide data to adc
  copierToPd.copy();
  // get data from dac
  copierFromPd.copy();
}

