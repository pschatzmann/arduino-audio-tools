#define USE_A2DP
#include "AudioTools.h"
#include "AudioDevices/ESP32AudioKit/AudioKit.h"

BluetoothA2DPSink a2dp_sink;
AudioKitStream kit;

// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    kit.write(data, length);
}

void setup() {
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.default_volume = 50;
  kit.begin(cfg);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.start("AudioKit");
}

void loop() {}