/**
 * @file example-serial-send.ino
 * @author Phil Schatzmann
 * @brief Sending encoded audio over ESPNow
 * @version 0.1
 * @date 2022-03-09
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "AudioTools/Communication/ESPNowStream.h"
#include "AudioTools/AudioCodecs/CodecSBC.h"

AudioInfo info(32000,1,16);
SineWaveGeneratorT<int16_t> sineWave( 32000);  // subclass of SoundGeneratorT with max amplitude of 32000
GeneratedSoundStreamT<int16_t> sound( sineWave); // Stream generated from sine wave
ESPNowStream now;
SBCEncoder sbc;
EncodedAudioStream encoder(&now, &sbc); // encode and write to ESP-now
StreamCopy copier(encoder, sound);  // copies sound into i2s
const char *peers[] = {"A8:48:FA:0B:93:01"};

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  auto cfg = now.defaultConfig();
  cfg.mac_address = "A8:48:FA:0B:93:02";
  now.begin(cfg);
  now.addPeers(peers);

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // start encoder
  encoder.begin(info);
  
  Serial.println("Sender started...");
}

void loop() { 
  copier.copy();
}