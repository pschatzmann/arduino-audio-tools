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
VS1053Config cfg;
QueueStream<uint8_t> buffer(1024,10);
StreamCopy copier(out, buffer); // copies sound into i2s

// Write data from A2DP to buffer
void read_data_stream(const uint8_t *data, uint32_t frames) {
  int frameSize = 4;
  buffer.write(data, frames*frameSize);
}


// for esp_a2d_audio_state_t see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2dp.html#_CPPv421esp_a2d_audio_state_t
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr){
  Serial.println(a2dp_sink.to_str(state));
    switch(state){
      case ESP_A2D_AUDIO_STATE_STARTED:
        buffer.begin();
        out.begin(cfg);
        break;
      case ESP_A2D_AUDIO_STATE_STOPPED:
      case ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND:
        out.end();
        buffer.end();
        buffer.clear();
        break;
    }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup VS1053
  cfg = out.defaultConfig();
  cfg.sample_rate = 44100;
  cfg.channels = 2;
  cfg.bits_per_sample = 16;
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;

  // register callbacks
  a2dp_sink.set_stream_reader(read_data_stream, false);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  // Start Bluetooth Audio Receiver
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-vs1053");


}

void loop() { 
  if (buffer) copier.copy();
}