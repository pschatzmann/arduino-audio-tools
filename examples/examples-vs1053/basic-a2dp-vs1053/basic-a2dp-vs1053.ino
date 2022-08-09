/**
 * @file basic-a2dp-audiovs1053.ino
 * @brief A2DP Sink with output to vs1053. The vs1053 expects encoded data so we encode the stream
 * to a WAV file
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

// install https://github.com/baldram/ESP_VS1053_Library.git

#include "AudioTools.h"
#include "AudioLibs/VS1053Stream.h"
#include "BluetoothA2DPSink.h"

#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4


BluetoothA2DPSink a2dp_sink;
VS1053Stream vs1053(VS1053_CS,VS1053_DCS, VS1053_DREQ); // final output
EncodedAudioStream out(&vs1053, new WAVEncoder()); // output is WAV file

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
  out.begin(cfg);

  vs1053.begin();
}

void loop() { delay(1000); }