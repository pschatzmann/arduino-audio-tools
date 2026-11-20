/**
 * @file audio-ble-sink.ino
 * @brief LE Audio sink: receives audio from a phone/PC (or audio-ble-source)
 * and plays it on the ESP32-S31 Function CoreBoard 1 (ES8311 DAC + amplifier).
 *
 * The codec is driven with I2SCodecStream and arduino-audio-driver's
 * GenericES8311 (no predefined pins): Wire is set up manually for the codec
 * I2C bus, and the I2S pins are set on the stream config. The variant
 * provides the pin macros (I2S_*, SDA1, SCL1, PA_CTRL).
 *
 * Pair with: audio-ble-source, or a phone/PC with LE Audio (unicast).
 * The peer chooses the format (16, 24 or 48 kHz, mono or stereo); the codec
 * follows it through the audio info notifications.
 *
 * Dependencies:
 * - https://github.com/pschatzmann/arduino-audio-tools
 * - https://github.com/pschatzmann/arduino-audio-driver
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "AudioTools/Communication/AudioBLE.h"

AudioBLE ble;
I2SCodecStream out(GenericES8311);
StreamCopy copier(out, ble);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  AudioBLEConfig ble_cfg = ble.defaultConfig(RX_MODE);
  ble_cfg.name = "AudioBLE Sink";

  // amplifier on
  pinMode(PA_CTRL, OUTPUT);
  digitalWrite(PA_CTRL, HIGH);

  // I2C bus of the codec
  Wire.begin(SDA1, SCL1);

  // I2S output to the codec: the format of the BLE stream
  auto config = out.defaultConfig(TX_MODE);
  config.copyFrom(ble_cfg);
  config.pin_mck = I2S_MCLK;
  config.pin_bck = I2S_SCLK;
  config.pin_ws = I2S_LRCK;
  config.pin_data = I2S_DOUT;
  if (!out.begin(config)) {
    Serial.println("I2S codec begin failed");
    stop();
  }

  // the peer selects the format (16/24/48 kHz): the codec follows it
  ble.addNotifyAudioChange(out);

  // start advertising: the peer can connect and stream audio to us
  if (!ble.begin(ble_cfg)) {
    Serial.println("AudioBLE begin failed");
  }
}

void loop() {
  copier.copy();
}
