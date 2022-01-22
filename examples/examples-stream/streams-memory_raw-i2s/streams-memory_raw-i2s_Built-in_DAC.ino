/**
 * @file streams-memory_raw-i2s_Built-in_DAC.ino
 * @author Phil Schatzmann
 * @brief Compile with Partition Scheme Huge APP!
 * @version 0.1
 * @date 2021-01-24
 *
 * @copyright Copyright (c) 2021
 * modify and test by @riraosan
 */
#include "AudioTools.h"
#include "StarWars30.h"

using namespace audio_tools;

uint8_t channels     = 2;
uint16_t sample_rate = 22050;

MemoryStream music(StarWars30_raw, StarWars30_raw_len);
I2SStream i2s;                            // Output to I2S
StreamCopyT<int16_t> copier(i2s, music);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  I2SConfig config       = i2s.defaultConfig(TX_MODE);
  config.sample_rate     = sample_rate;
  config.channels        = channels;
  config.bits_per_sample = 16;
  config.i2s_format      = I2S_MSB_FORMAT;
  config.is_digital      = false; // use Built-in DAC and GPIO25,26 output
  i2s.begin(config);
}

void loop() {
  if (!copier.copy2()) {
    i2s.end();
    stop();
  }
}
