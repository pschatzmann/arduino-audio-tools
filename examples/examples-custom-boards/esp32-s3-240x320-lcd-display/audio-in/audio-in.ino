/**
 * @file audio-in.ino
 * @brief Microphone test for the Hosyond 2.8" ESP32-S3 Display: reads the
 * on-board MEMS mic via the ES8311 codec's ADC path and prints the PCM
 * samples to Serial as CSV - open the Arduino IDE's Serial Plotter to see
 * the waveform, or just watch for non-zero values while talking near the
 * board to confirm the mic path is alive.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @see https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(16000, 2, 16);
AudioBoardStream in(ESP32S3HosyondDisplay);
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
