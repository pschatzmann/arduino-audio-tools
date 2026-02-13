/**
 * @file communication-broadcast-espnow-send.ino
 * @author Phil Schatzmann
 * @brief Broadcasting audio over ESPNow. We need to use Throttle to avoid buffer
 * overflows on the receiver side.
 * @version 0.1
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 */

#include "AudioTools.h"
#include "AudioTools/Communication/ESPNowStream.h"

AudioInfo info(8000, 1, 16);
SineWaveGenerator<int16_t> sineWave( 32000);  // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound( sineWave); // Stream generated from sine wave
ESPNowStream now;
Throttle throttle(now);
StreamCopy copier(throttle, sound);  // copies sound into i2s

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:02";
  cfg.use_send_ack = false; // broadcast does not support ack
  now.begin(cfg);
  // optional, if no peer is defined we use broadcast
  // now.addBroadcastPeer();

  // Setup sine wave
  sineWave.begin(info, N_B4);
  Serial.println("Sender started...");

  // setup throttle
  throttle.begin(info);
}

void loop() { 
  copier.copy();
}