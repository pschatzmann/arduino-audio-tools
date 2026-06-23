/**
 * @file communication-nrf24-receive.ino
 * @author Phil Schatzmann
 * @brief Receiving audio over NRF24L01(+) module and outputting to I2S.
 * Uses no ACK and no CRC to match the sender settings.
 * Tested with Teensy 4.1, ESP32 and Arduino Nano.
 * @version 0.1
 * @date 2024-01-01
 *
 * @copyright Copyright (c) 2024
 */

#include "AudioTools.h"
#include "AudioTools/Communication/NRF24Stream.h"

AudioInfo info(32000, 1, 16);
NRF24Stream radio;
I2SStream i2s;
StreamCopy copier(i2s, radio);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while (!Serial);
  Serial.println("starting...");

  // Setup NRF24 as receiver
  auto cfg = radio.defaultConfig(RX_MODE);
  // cfg.pin_ce = 9;      // adjust for your board
  // cfg.pin_csn = 10;    // adjust for your board
  // cfg.channel = 124;   // must match sender
  cfg.copyFrom(info);
  if (!radio.begin(cfg)) {
    Serial.println("NRF24 failed");
    stop();
  }

  // Setup I2S output
  auto i2s_cfg = i2s.defaultConfig(TX_MODE);
  i2s_cfg.copyFrom(info);
  i2s.begin(i2s_cfg);

  Serial.println("Receiver started...");
}

void loop() {
  copier.copy();
}
