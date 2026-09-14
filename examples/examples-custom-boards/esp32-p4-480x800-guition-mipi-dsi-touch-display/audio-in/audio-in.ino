/**
 * @file audio-in.ino
 * @brief Microphone test for the Guition JC4880P443C_I_W (ESP32-P4):
 * reads the ES8311 codec's ADC path and prints PCM samples to Serial as
 * CSV - open the Arduino IDE's Serial Plotter to see the waveform, or
 * just watch for non-zero values while talking near the board.
 *
 * Uses arduino-audio-driver's `GenericES8311` (no predefined pins) - see
 * audio-out.ino for why, and for this board's I2S/I2C pins.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"

// Pins from guition-jc4880p4-bsp's board_p4_pins.h "Audio" section.
constexpr int kPinI2sMclk = 13;
constexpr int kPinI2sBck = 12;
constexpr int kPinI2sWs = 10;
constexpr int kPinI2sDin = 48;   // codec -> P4, the mic-in path this file reads
constexpr int kPinCodecScl = 8;  // shared with GT911 touch bus
constexpr int kPinCodecSda = 7;

AudioInfo info(16000, 2, 16);
I2SCodecStream in(GenericES8311);
CsvOutput<int16_t> csvOutput(Serial);
StreamCopy copier(csvOutput, in);  // copies mic data to Serial as CSV

void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);

  Wire.begin(kPinCodecSda, kPinCodecScl);

  Serial.println("starting I2S...");
  auto config = in.defaultConfig(RX_MODE);
  config.copyFrom(info);
  config.pin_mck = kPinI2sMclk;
  config.pin_bck = kPinI2sBck;
  config.pin_ws = kPinI2sWs;
  config.pin_data = kPinI2sDin;  // RX pin in RX_MODE
  if (!in.begin(config)) {
    Serial.println("error!");
    stop();
  }

  csvOutput.begin(info);
  Serial.println("started...");
}

void loop() { copier.copy(); }
