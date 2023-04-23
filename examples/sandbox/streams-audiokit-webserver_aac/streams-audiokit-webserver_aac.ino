/**
 * @file streams-audiokit-webserver_aac.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as AAC stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioKit.h"
#include "AudioCodecs/CodecAACFDK.h"

AudioKitStream kit;    
AACEncoderFDK *fdk=nullptr;
AudioEncoderServer *server=nullptr;  

// Arduino setup
void setup(){
  Serial.begin(115200);
  // Defining Loglevels for the different libraries
  //AudioLogger::instance().begin(Serial, AudioLogger::Info);
  //LOGLEVEL_FDK = FDKInfo; 
  //LOGLEVEL_AUDIOKIT = AudioKitInfo;
  
  // setup and configure fdk
  fdk = new AACEncoderFDK();  
  fdk->setAudioObjectType(2);  // AAC low complexity
  fdk->setOutputBufferSize(1024); // decrease output buffer size
  fdk->setVariableBitrateMode(2); // low variable bitrate
  server = new AudioEncoderServer(fdk,"WIFI","password");  

  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = AUDIO_HAL_ADC_INPUT_LINE2;
  config.sample_rate = 44100; 
  config.default_actions_active = false; 
  config.channels = 2; 
  config.sd_active = false;
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
