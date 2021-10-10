/**
 * @file streams-analog-serial.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/streams-analog-serial/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


#include "Arduino.h"
#include "AudioTools.h"

using namespace audio_tools;  

const uint16_t sample_rate = 44100;
const uint8_t channels = 2;
AnalogAudioStream in; 
CsvStream<int16_t> out(Serial, channels); // ASCII output stream 
StreamCopy copier(out, in); // copy i2sStream to a2dpStream
ConverterAutoCenter<int16_t> center; // set avg to 0

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // RX automatically uses port 0 with pins GPIO34,GPIO35
  auto cfgRx = in.defaultConfig(RX_MODE);
  cfgRx.sample_rate = sample_rate;
  in.begin(cfgRx);

  // open output
  out.begin();

}

// Arduino loop - copy data 
void loop() {
  copier.copy(center);
}