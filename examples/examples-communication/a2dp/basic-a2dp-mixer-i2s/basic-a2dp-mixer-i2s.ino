/**
 * @file basic-a2dp-mixer-i2s.ino
 * @brief A2DP Sink with output to mixer: I think this is the most efficient way
 * of mixing a signal that is coming from A2DP which requires only 160 byte of additional RAM.
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"

AudioInfo info(44100, 2, 16);
BluetoothA2DPSink a2dp_sink;
I2SStream i2s;
SineWaveGenerator<int16_t> sineWave(10000);  // subclass of SoundGenerator with max amplitude of 10000
GeneratedSoundStream<int16_t> sound(sineWave); // Stream generated from sine wave
OutputMixer<int16_t> mixer(i2s, 2);  // output mixer with 2 outputs
const int buffer_size = 80;  // split up the output into small chunks
uint8_t sound_buffer[buffer_size];

// Write data to mixer
void read_data_stream(const uint8_t *data, uint32_t length) {
  // To keep the mixing buffer small we split up the output into small chunks
  int count = length / buffer_size + 1;
  for (int j = 0; j < count; j++) {
    const uint8_t *start = data + (j * buffer_size);
    const uint8_t *end = min(data + length, start + buffer_size);
    int len = end - start;
    if (len > 0) {
      // write a2dp
      mixer.write(start, len);

      // write sine tone with identical length
      sound.readBytes(sound_buffer, len);
      mixer.write(sound_buffer, len);

      // We could flush to force the output but this is not necessary because we
      // were already writing all 2 streams mixer.flushMixer();
    }
  }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // setup Output mixer with min necessary memory
  mixer.begin(buffer_size);

  // Register data callback
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // Start Bluetooth Audio Receiver
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-i2s");

  // Update sample rate
  info.sample_rate = a2dp_sink.sample_rate();

  // start sine wave
  sineWave.begin(info, N_B4);

  // setup output
  auto cfg = i2s.defaultConfig();
  cfg.copyFrom(info);
  // cfg.pin_data = 23;
  cfg.buffer_count = 8;
  cfg.buffer_size = 256;
  i2s.begin(cfg);
}

void loop() { delay(100); }