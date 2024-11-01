/**
 * @file basic-i2s-a2dp.ino
 * @author Phil Schatzmann
 * @brief We use a INMP441 I2S microphone as input and send the data to A2DP
 * Unfortunatly the data type from the microphone (int32_t)  does not match with
 * the required data type by A2DP (int16_t), so we need to convert.
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/A2DPStream.h"

AudioInfo info32(44100, 2, 32);
AudioInfo info16(44100, 2, 16);
BluetoothA2DPSource a2dp_source;
I2SStream i2s;
FormatConverterStream conv(i2s);
const int BYTES_PER_FRAME = 4;


int32_t get_sound_data(Frame* data, int32_t frameCount) {
  return conv.readBytes((uint8_t*)data, frameCount*BYTES_PER_FRAME)/BYTES_PER_FRAME;
}

// Arduino Setup
void setup(void) {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup conversion
  conv.begin(info32, info16);

  // start i2s input with default configuration
  Serial.println("starting I2S...");
  auto cfg = i2s.defaultConfig(RX_MODE);
  cfg.i2s_format = I2S_STD_FORMAT; // or try with I2S_LSB_FORMAT
  cfg.copyFrom(info32);
  cfg.is_master = true;
  i2s.begin(cfg);

  // start the bluetooth
  Serial.println("starting A2DP...");
  // a2dp_source.set_auto_reconnect(false);
  a2dp_source.start("LEXON MINO L", get_sound_data);

  Serial.println("A2DP started");
}

// Arduino loop - repeated processing
void loop() { delay(1000); }