/**
 * @file streams-audiokit-webserver_mp3.ino
 *
 *  This sketch reads sound data from the AudioKit. The result is provided as MP3 stream which can be listened to in a Web Browser
 *
 * @author Phil Schatzmann, Thorsten Godau (changed AAC example to MP3, added optional static IP)
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioCodecs/CodecMP3LAME.h"

// Set static IP address and stuff (optional)
IPAddress IPA_address(192, 168, 0, 222);
IPAddress IPA_gateway(192, 168, 0, 1);
IPAddress IPA_subnet(255, 255, 0, 0);
IPAddress IPA_primaryDNS(192, 168, 0, 1);  //optional
IPAddress IPA_secondaryDNS(8, 8, 8, 8);    //optional

// WIFI
const char *ssid = "ssid";
const char *password = "password";

AudioInfo info(16000,1,16);
MP3EncoderLAME mp3;
AudioEncoderServer server(&mp3, ssid, password);
AudioBoardStream kit(AudioKitEs8388V1);    

// Arduino setup
void setup(){
  Serial.begin(115200);
  // Defining Loglevels for the different libraries
  //AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  //LOGLEVEL_AUDIOKIT = AudioKitInfo;

  // Configures static IP address (optional)
  if (!WiFi.config(IPA_address, IPA_gateway, IPA_subnet, IPA_primaryDNS, IPA_secondaryDNS))
  {
    Serial.println("WiFi.config: Failed to configure static IPv4...");
  }
 
  // start i2s input with default configuration
  Serial.println("starting AudioKit...");
  auto config = kit.defaultConfig(RX_MODE);
  config.input_device = ADC_INPUT_LINE2;
  config.copyFrom(info); 
  config.sd_active = false;
  kit.begin(config);
  Serial.println("AudioKit started");

  // start data sink
  server.begin(kit, config);
  Serial.println("Server started");

}

// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
