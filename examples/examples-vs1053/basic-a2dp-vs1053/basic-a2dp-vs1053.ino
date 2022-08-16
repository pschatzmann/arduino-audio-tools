/**
 * @file basic-a2dp-audiovs1053.ino
 * @brief A2DP Sink with output to vs1053. The vs1053 expects encoded data so we encode the stream
 * to a WAV file
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// install https://github.com/pschatzmann/arduino-vs1053.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
VS1053Stream out; // final output

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
  a2dp_sink.start("a2dp-vs1053");

  // setup output
  auto cfg = out.defaultConfig();
  cfg.sample_rate = a2dp_sink.sample_rate();
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  // Use your custom pins
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;

  out.begin(cfg);
}

void loop() { delay(1000); }