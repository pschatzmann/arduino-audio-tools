/**
 * @file basic-a2dp-audiokit.ino
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-audiokit/basic-a2dp-audiokit/README.md
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioA2DP.h"


BluetoothA2DPSink a2dp_sink;
AudioKitStream kit;
Equilizer3Bands eq(kit);
ConfigEquilizer3Bands cfg_eq;


// Write data to AudioKit in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
    eq.write(data, length);
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.sd_active = false;
  kit.begin(cfg);

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
  // kit.processActions();  // uncomment for default button commands
}