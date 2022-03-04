/**
 * @file streams-analog-i2s.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-adc-i2s/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"

const uint16_t sample_rate = 44100;
AnalogAudioStream in; 
I2SStream out;                        
StreamCopy copier(out, in); // copy in to out
ConverterAutoCenter<int16_t> converter;

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // RX automatically uses port 0 with pin GPIO34
  auto cfgRx = in.defaultConfig(RX_MODE);
  cfgRx.sample_rate = sample_rate;
  in.begin(cfgRx);
 
  // TX on I2S_NUM_1 
  auto cfgTx = out.defaultConfig(TX_MODE);
  cfgTx.port_no = 1;
  cfgTx.sample_rate = sample_rate;
  out.begin(cfgTx);
}

// Arduino loop - copy data 
void loop() {
  copier.copy(converter);
}