/**
 * @file basic-a2dp-fft.ino
 * @brief A2DP Sink with output to FFT.   
 * For details see the FFT Wiki: https://github.com/pschatzmann/arduino-audio-tools/wiki/FFT
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // or any other supported inplementation
#include "BluetoothA2DPSink.h"

BluetoothA2DPSink a2dp_sink;
AudioRealFFT fft; // or any other supported inplementation

// Provide data to FFT
void writeDataStream(const uint8_t *data, uint32_t length) {
  fft.write(data, length);
}

// display fft result
void fftResult(AudioFFTBase &fft){
    float diff;
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
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Setup FFT
  auto tcfg = fft.defaultConfig();
  tcfg.length = 4096;
  tcfg.channels = 2;
  tcfg.sample_rate = a2dp_sink.sample_rate();;
  tcfg.bits_per_sample = 16;
  tcfg.callback = &fftResult;
  fft.begin(tcfg);

  // register callback
  a2dp_sink.set_stream_reader(writeDataStream, false);

  // Start Bluetooth Audio Receiver
  Serial.print("starting a2dp-fft...");
  a2dp_sink.set_auto_reconnect(false);
  a2dp_sink.start("a2dp-fft");

}

void loop() { delay(100); }