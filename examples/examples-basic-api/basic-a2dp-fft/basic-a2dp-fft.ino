/**
 * @file basic-a2dp-fft.ino
 * @brief A2DP Sink with output to FFT.   
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "BluetoothA2DPSink.h"
#include "AudioTools.h"
#include "AudioLibs/AudioEspressifFFT.h" // or AudioKissFFT

BluetoothA2DPSink a2dp_sink;
AudioEspressifFFT fft; // or AudioKissFFT

// Write data to SPDIF in callback
void read_data_stream(const uint8_t *data, uint32_t length) {
  fft.write(data, length);
}

// display fft result
void fftResult(AudioFFTBase &fft){
    int diff;
    auto result = fft.result();
    if (result.magnitude>100){
        Serial.print(result.frequency);
        Serial.print(" ");
        Serial.print(result.magnitude);  
        Serial.print(" => ");
        Serial.print(result.frequencyAsNote(diff));
        Serial.print( " diff: ");
        Serial.println(diff);
    }
}

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 4096;
  tcfg.channels = 2;
  tcfg.sample_rate = a2dp_sink.sample_rate();;
  tcfg.bits_per_sample = 16;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);

  // register callback
  a2dp_sink.set_stream_reader(read_data_stream, false);

  // Start Bluetooth Audio Receiver
  Serial.print("starting a2dp-fft...");
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-fft");

}

void loop() { delay(100); }