/**
 * @file streams-audiokit-webserver_mp3.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as mp3 stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
#define USE_LAME

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"

AudioKitStream kit;    
AudioEncoderServer *server=new AudioEncoderServer(new MP3EncoderLAME(),"Phil Schatzmann","sabrina01");    

// Arduino setup
void setup(){
  Serial.begin(115200);
  // Defining Loglevels for the different libraries
  //AudioLogger::instance().begin(Serial, AudioLogger::Info);
  //LOGLEVEL_lame = lameInfo; 
  //LOGLEVEL_AUDIOKIT = AudioKitInfo;
  
  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  config.sample_rate = 44100; 
  config.default_actions_active = false; 
  config.channels = 2; 
  kit.begin(config);
  Serial.println("AudioKit started");

  // start data sink
  server->begin(kit, config);
  Serial.println("Server started");

}

// Arduino loop  
void loop() {
  // Handle new connections
  server->doLoop();  
}
