/**
 * @file basic-a2dp-audiospdif.ino
 * @brief A2DP Sink with output to SPDIFOutput
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#include "AudioTools.h"
#include "AudioLibs/SPDIFOutput.h"
#include "BluetoothA2DPSink.h"

AudioInfo info(44100, 2, 16);
SPDIFOutput spdif;
BluetoothA2DPSink a2dp_sink(spdif);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // setup output
  auto cfg = spdif.defaultConfig();
  cfg.copyFrom(info);
  cfg.buffer_size = 384;
  cfg.buffer_count = 30;
  cfg.pin_data = 23;
  spdif.begin(cfg);

  // Start Bluetooth Audio Receiver
  a2dp_sink.start("a2dp-spdif");

}

void loop() {
  delay(100);
}
