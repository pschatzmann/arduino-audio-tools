/**
 * @file streams-analog-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-adc-serial/README.md
 * @copyright GPLv3
 * #TODO retest is outstanding
 */

#include "Arduino.h"
#include "AudioTools.h"

const uint16_t sample_rate = 8000;
AnalogAudioStream in; 
int channels = 1;
CsvStream<int16_t> out(Serial, channels); // ASCII output stream 
StreamCopy copier(out, in); // copy i2sStream to CsvStream
ConverterAutoCenter<int16_t> center(2); // set avg to 0

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // RX automatically uses port 0 with pins GPIO34,GPIO35
  auto cfgRx = in.defaultConfig(RX_MODE);
  cfgRx.sample_rate = sample_rate;
  cfgRx.channels = channels;
  in.begin(cfgRx);

  // open output
  out.begin();

}

// Arduino loop - copy data 
void loop() {
  copier.copy(center);
}