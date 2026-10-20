/**
 * @file audio-in.ino
 * @brief Microphone test for the Waveshare ESP32-P4-WIFI6: reads the
 * ES8311 codec's ADC path (onboard SMD microphone) and prints PCM samples
 * to Serial as CSV - open the Arduino IDE's Serial Plotter to see the
 * waveform, or just watch for non-zero values while talking near the board.
 *
 * Uses arduino-audio-driver's `WaveshareESP32P4WIFI6` board, which
 * defines the codec I2C and I2S pins.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"

AudioInfo info(16000, 2, 16);
I2SCodecStream in(WaveshareESP32P4WIFI6);
CsvOutput<int16_t> csvOutput(Serial);
StreamCopy copier(csvOutput, in);  // copies mic data to Serial as CSV

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  Serial.println("starting I2S...");
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  if (!in.begin(config)) {
    Serial.println("error!");
    stop();
  }

  csvOutput.begin(info);
  Serial.println("started...");
}

void loop() { copier.copy(); }
