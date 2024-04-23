/**
 * @file streams-audiokit-webserver_wav.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as WAV stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"

AudioEncoderServer server(new WAVEncoder(),"ssid","password");  
AudioBoardStream kit(AudioKitEs8388V1);    

// Arduino setup
void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = ADC_INPUT_LINE1;
  config.sample_rate = 44100;
  config.sd_active = false;
  kit.begin(config);
  Serial.println("AudioKit started");

  // start data sink
  server.begin(kit, config);
}

// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
