/**
 * @file basic-a2dp-audiospdif.ino
 * @brief A2DP Sink with output to SPDIFOutput
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioConfigLocal.h"
#include "BluetoothA2DPSink.h"
#include "AudioTools.h"

AudioInfo info(44100, 2, 16);
BluetoothA2DPSink a2dp_sink;
SPDIFOutput spdif;

// Write data to SPDIF in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    spdif.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup output
  auto cfg = spdif.defaultConfig();
  cfg.copyFrom(info);
  cfg.pin_data = 23;
  spdif.begin(cfg);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // Start Bluetooth Audio Receiver
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-spdif");

}

void loop() {
  delay(100);
}