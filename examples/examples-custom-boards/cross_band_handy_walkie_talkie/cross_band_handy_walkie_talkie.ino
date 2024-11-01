/**
   @file streams-generator-audiokit.ino
   @brief Tesing I2S output on the cross band handy walkie talkie
   https://github.com/immortal-sniper1/cross_band_handy_walkie_talkie

   @author Phil Schatzmann
   @copyright GPLv3
*/

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "SD.h"

AudioInfo info(32000, 2, 16);
SineWaveGenerator<int16_t> sineWave(
    32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound(
    sineWave);  // Stream generated from sine wave
DriverPins my_pins;
AudioBoard board(AudioDriverES8388, my_pins);
AudioBoardStream out(board);
StreamCopy copier(out, sound);  // copies sound into i2s

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  while (!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // sd pins: clk,  miso,  mosi,cs,
  my_pins.addSPI(ESP32PinsSD{PinFunction::SD, 44, 42, 43, 2, SPI});
  // add i2c codec pins: scl, sda, port, frequency
  my_pins.addI2C(PinFunction::CODEC, 35, 36);
  // add i2s pins: mclk, bck, ws,data_out, data_in ,(port)
  my_pins.addI2S(PinFunction::CODEC, 47, 21, 12, 14, 11);

  // start I2S
  Serial.println("starting I2S...");
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(info);
  config.sd_active = true;
  out.begin(config);

  // check SD drive
  if (!SD.begin(2)) {
    Serial.println("Card Mount Failed");
    stop();
  }

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("started...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }