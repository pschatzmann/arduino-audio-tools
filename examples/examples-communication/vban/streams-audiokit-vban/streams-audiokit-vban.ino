/**
 * @file streams-i2s-vban.ino
 * @author Phil Schatzmann
 * @brief sends signal from i2s (using an AudioKit) to VBAN Receptor App 
 */

#include "AudioTools.h"
#include "AudioLibs/VBANStream.h"
#include "AudioLibs/AudioBoardStream.h" // comment out when not using AudioKit

AudioInfo info(44100, 2, 16);
AudioBoardStream in(AudioKitEs8388V1);  // Audio source e.g. replace with I2SStream
VBANStream out;
StreamCopy copier(out, in, 2048);                             // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // setup output
  auto cfg = out.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.ssid = "ssid";
  cfg.password = "password";
  cfg.stream_name = "Stream1";
  cfg.target_ip = IPAddress{192,168,1,37}; // comment out to broadcast
  cfg.throttle_active = false; // generator is much too fast, we need to stall
  if (!out.begin(cfg)) stop();

  // Setup input from mic 
  // setup input
  auto cfg_in = in.defaultConfig(RX_MODE);
  cfg_in.sd_active = false;
  cfg_in.buffer_size = 256;
  cfg_in.buffer_count = 4;
  cfg_in.copyFrom(info);
  cfg_in.input_device = ADC_INPUT_LINE2; // microphone
  in.begin(cfg_in);
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}