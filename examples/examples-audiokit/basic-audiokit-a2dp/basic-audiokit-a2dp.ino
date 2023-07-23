/**
 * @file base-audiokit-a2dp.ino
 * @author Phil Schatzmann
 * @brief We play the input from the ADC to an A2DP speaker
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioLibs/AudioA2DP.h"

BluetoothA2DPSource a2dp_source;
AudioKitStream i2s;
const int16_t BYTES_PER_FRAME = 4;

// callback used by A2DP to provide the sound data - usually len is 128 2 channel int16 frames
int32_t get_sound_data(Frame* data, int32_t frameCount) {
  return i2s.readBytes((uint8_t*)data, frameCount*BYTES_PER_FRAME)/BYTES_PER_FRAME;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto cfg = i2s.defaultConfig(RX_MODE);
  cfg.i2s_format = I2S_STD_FORMAT; // or try with I2S_LSB_FORMAT
  cfg.bits_per_sample = 16;
  cfg.channels = 2;
  cfg.sample_rate = 44100;
  cfg.input_device = AUDIO_HAL_ADC_INPUT_LINE2; // microphone
  i2s.begin(cfg);

  // start the bluetooth
  Serial.println("starting A2DP...");
  a2dp_source.start("LEXON MINO L", get_sound_data);  
}

// Arduino loop - repeated processing 
void loop() {
  delay(1000);
}
