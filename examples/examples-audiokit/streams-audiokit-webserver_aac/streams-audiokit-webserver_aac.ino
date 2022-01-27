/**
 * @file streams-audiokit-webserver_aac.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as AAC stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#define USE_FDK

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioEncoderServer server(new AACEncoderFDK(),"ssid","password");  
AudioKitStream kit;    

// Arduino setup
void setup(){
  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Warning);

  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = AUDIO_HAL_ADC_INPUT_LINE1;
  config.sample_rate = 44100;
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
