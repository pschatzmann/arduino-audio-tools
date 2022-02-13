/**
 * @file basic-a2dp-audioi2s.ino
 * @brief A2DP Sink with output to respampled I2S
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#define USE_A2DP
#include "AudioConfigLocal.h"
#include "AudioTools.h"

BluetoothA2DPSink a2dp_sink;
I2SStream i2s;
Resample<int16_t> out(i2s, 2, 2);

// Write data to SPDIF in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    out.write(data, length);
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
  cfg.pin_bck = 26;
  cfg.pin_ws = 25;
  cfg.pin_data = 22;
  cfg.sample_rate = a2dp_sink.sample_rate()*2;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  i2s.begin(cfg);

}

void loop() {
  delay(100);
}