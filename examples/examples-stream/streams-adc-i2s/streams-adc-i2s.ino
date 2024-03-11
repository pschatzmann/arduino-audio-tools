/**
 * @file streams-analog-i2s.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-adc-i2s/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
AnalogAudioStream in; 
I2SStream out;                        
StreamCopy copier(out, in); // copy in to out

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // RX automatically uses port 0 with pin GPIO34
  auto cfgRx = in.defaultConfig(RX_MODE);
  cfgRx.copyFrom(info);
  in.begin(cfgRx);
 
  // TX on I2S_NUM_1 
  auto cfgTx = out.defaultConfig(TX_MODE);
  cfgTx.port_no = 1;
  cfgTx.copyFrom(info);
  out.begin(cfgTx);
}

// Arduino loop - copy data 
void loop() {
  copier.copy();
}