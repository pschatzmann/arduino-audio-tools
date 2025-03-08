/**
 * @file basic-a2dp-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/basic-a2dp-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h" // install https://github.com/pschatzmann/ESP32-A2DP
#include "AudioTools/AudioLibs/AudioBoardStream.h" // install https://github.com/pschatzmann/arduino-audio-driver


BluetoothA2DPSink a2dp_sink;
AudioBoardStream kit(AudioKitEs8388V1); 
Equalizer3Bands eq(kit);
ConfigEqualizer3Bands cfg_eq;


// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    eq.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);

  // max volume
  kit.setVolume(1.0);

  // setup equilizer
  cfg_eq = eq.defaultConfig();
  cfg_eq.setAudioInfo(cfg); // use channels, bits_per_sample and sample_rate from kit
  cfg_eq.gain_low = 0.5; 
  cfg_eq.gain_medium = 0.5;
  cfg_eq.gain_high = 1.0;
  eq.begin(cfg_eq);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.start("AudioKit");
}

void loop() {
}