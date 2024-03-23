/**
 * @file streams-mixer-audiokit.ino
 * @author Phil Schatzmann
 * @brief Simple Demo for mixing 2 input streams
 * @version 0.1
 * @date 2022-11-15
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "drums.h"
#include "guitar.h"

InputMixer<int16_t> mixer;
AudioBoardStream kit(AudioKitEs8388V1);
MemoryStream drums(drums_raw, drums_raw_len);
MemoryStream guitar(guitar_raw, guitar_raw_len);
StreamCopy copier(kit, mixer);

void setup() {
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);  

  // auto restart when MemoryStream has ended
  drums.setLoop(true);
  guitar.setLoop(true);

  // setup output
  auto cfg = kit.defaultConfig(TX_MODE);
  cfg.channels = 1;
  cfg.sample_rate = 8000;
  kit.begin(cfg);
  // max volume
  kit.setVolume(1.0);

  mixer.add(drums);
  mixer.add(guitar);
  mixer.begin(cfg);

}

void loop() { copier.copy(); }
