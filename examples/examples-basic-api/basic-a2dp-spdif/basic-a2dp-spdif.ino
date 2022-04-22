/**
 * @file basic-a2dp-audiospdif.ino
 * @brief A2DP Sink with output to SPDIFStream
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioConfigLocal.h"
#include "BluetoothA2DPSink.h"
#include "AudioTools.h"

BluetoothA2DPSink a2dp_sink;
SPDIFStream spdif;

// Write data to SPDIF in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    spdif.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);
  
  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // Start Bluetooth Audio Receiver
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-spdif");

  // setup output
  auto cfg = spdif.defaultConfig();
  cfg.pin_data = 23;
  cfg.sample_rate = a2dp_sink.sample_rate();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  spdif.begin(cfg);

}

void loop() {
  delay(100);
}