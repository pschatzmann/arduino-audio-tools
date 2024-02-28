/**
 * Test sketch for test.pd that was compiled with hvcc -n test test.pd
 */

#include "AudioLibs/AudioBoardStream.h"  // install https://github.com/pschatzmann/arduino-audio-driver
#include "AudioLibs/PureDataStream.h"
#include "AudioTools.h"
#include "Heavy_test.hpp"  // import before PureDataStream!

Heavy_test pd_test(44100);
PureDataStream pd(pd_test);
AudioBoardStream out(AudioKitEs8388V1);
StreamCopy copier(out, pd);  // copy kit to kit

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup pd
  pd.begin();

  // setup output
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  cfg.copyFrom(pd.audioInfo());
  out.begin(cfg);
}

void loop() { copier.copy(); }