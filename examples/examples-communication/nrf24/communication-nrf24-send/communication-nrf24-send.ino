/**
 * @file communication-nrf24-send.ino
 * @author Phil Schatzmann
 * @brief Sending audio over NRF24L01(+) module. Uses writeFast() with no ACK
 * and no CRC for lowest latency. Tested with Teensy 4.1, ESP32 and Arduino
 * Nano.
 * @version 0.1
 * @date 2024-01-01
 *
 * @copyright Copyright (c) 2024
 */

#include "AudioTools.h"
#include "AudioTools/Communication/NRF24Stream.h"

AudioInfo info(32000, 1, 16);
SineGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
NRF24Stream radio;
StreamCopy copier(radio, sound);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  while (!Serial);
  Serial.println("starting...");

  // Setup NRF24 as transmitter
  auto cfg = radio.defaultConfig(TX_MODE);
  // cfg.pin_ce = 9;      // adjust for your board
  // cfg.pin_csn = 10;    // adjust for your board
  // cfg.channel = 124;   // upper band, less interference
  cfg.copyFrom(info);
  if (!radio.begin(cfg)) {
    Serial.println("NRF24 failed");
    stop();
  }

  // Setup sine wave
  sineWave.begin(info, N_B4);

  Serial.println("Sender started...");
}

void loop() {
  copier.copy();
}
