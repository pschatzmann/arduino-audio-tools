/**
 * @file streams-analog-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-adc-serial/README.md
 * @copyright GPLv3
 * #TODO retest is outstanding
 */

#include "Arduino.h"
#include "AudioTools.h"

AnalogAudioStream in; 
AudioInfo info(8000, 1, 16);
CsvOutput<int16_t> out(Serial); // ASCII output stream 
StreamCopy copier(out, in); 

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  auto cfgRx = in.defaultConfig(RX_MODE);
  // cfgRx.start_pin = A1; // optinally define pin
  // cfgRx.is_auto_center_read = true;
  cfgRx.copyFrom(info);
  in.begin(cfgRx);

  // open output
  out.begin(info);

}

// Arduino loop - copy data 
void loop() {
  copier.copy();  // 
}
