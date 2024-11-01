/**
 * Test sketch for test.pd that was compiled with hvcc -n test test.pd
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"  // install https://github.com/pschatzmann/arduino-audio-driver
#include "Heavy_test.hpp"  // import before PureDataStream!
#include "AudioTools/AudioLibs/PureDataStream.h"

Heavy_test pd_test(44100);
PureDataStream pd(pd_test);
AudioBoardStream out(AudioKitEs8388V1); // or replace with other output
StreamCopy copier(out, pd);  // copy kit to kit

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup pd
  pd.begin();

  // setup output
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  cfg.copyFrom(pd.audioInfo());
  out.begin(cfg);
}

void loop() { copier.copy(); }