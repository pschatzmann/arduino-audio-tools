/**
 * @file basic-a2dp-audioi2s.ino
 * @brief A2DP Sink with output to I2SStream. This example is of small value
 * since my Bluetooth Library already provides I2S output out of the box.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
I2SStream i2s;

// Write data to I2S
void read_data_stream(const uint8_t *data, uint32_t length) {
  i2s.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // Start Bluetooth Audio Receiver
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-i2s");

  // setup output
  auto cfg = i2s.defaultConfig();
  cfg.pin_data = 23;
  cfg.sample_rate = a2dp_sink.sample_rate();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  cfg.buffer_count = 8;
  cfg.buffer_size = 256;
  i2s.begin(cfg);
}

void loop() { delay(100); }