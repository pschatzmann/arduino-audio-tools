/**
 * @file basic-a2dp-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/basic-a2dp-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/A2DPStream.h" // install https://github.com/pschatzmann/ESP32-A2DP
#include "AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver


BluetoothA2DPSink a2dp_sink;
AudioBoardStream kit(AudioKitEs8388V1); 

// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    kit.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.start("AudioKit");
}

void loop() {
}