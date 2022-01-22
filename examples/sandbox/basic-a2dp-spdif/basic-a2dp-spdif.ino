/**
 * @file basic-a2dp-audiospdif.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiospdif/basic-a2dp-audiospdif/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#define USE_A2DP
#include "AudioTools.h"
#include "AudioExperiments/AudioSPDIF.h"


BluetoothA2DPSink a2dp_sink;
SPDIFStream spdif;

// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    spdif.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = spdif.defaultConfig();
  cfg.pin_data = 22;
  spdif.begin(cfg);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.start("a2dp-spdif");
}

void loop() {
  delay(100);
}