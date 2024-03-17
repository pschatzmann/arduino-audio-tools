/**
 * @file streams-audiokit-webserver_aac.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as AAC stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioLibs/AudioBoardStream.h"
#include "AudioCodecs/CodecAACFDK.h"

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioInfo info(16000,1,16);
AACEncoderFDK fdk;
AudioEncoderServer server(&fdk, ssid, password);
AudioBoardStream kit(AudioKitEs8388V1);    

// Arduino setup
void setup(){
  Serial.begin(115200);
  // Defining Loglevels for the different libraries
  //AudioLogger::instance().begin(Serial, AudioLogger::Info);
  //LOGLEVEL_FDK = FDKInfo; 
  //LOGLEVEL_AUDIOKIT = AudioKitInfo;
  
  // setup and configure fdk (not necessary if you activate PSRAM)
  fdk.setAudioObjectType(2);  // AAC low complexity
  fdk.setOutputBufferSize(1024); // decrease output buffer size
  fdk.setVariableBitrateMode(2); // low variable bitrate

  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = ADC_INPUT_LINE2;
  config.copyFrom(info); 
  config.sd_active = false;
  kit.begin(config);
  Serial.println("AudioKit started");

  // start data sink
  server.begin(kit, info);
  Serial.println("Server started");

}

// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
