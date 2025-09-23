/**
 * @file streams-i2s-vban.ino
 * @author Phil Schatzmann
 * @brief sends signal from i2s (using an AudioKit) to VBAN Receptor App 
 */

#include "AudioTools.h"
#include "AudioTools/Communication/VBANStream.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h" // comment out when not using AudioKit

AudioBoardStream out(AudioKitEs8388V1);  // Audio source e.g. replace with I2SStream
VBANStream in;
StreamCopy copier(out, in);              // copies sound into i2s

// Arduino Setup
void setup(void) {  
  // Open Serial 
  Serial.begin(115200);
  while(!Serial);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // setup output
  auto cfg_out = out.defaultConfig(TX_MODE);
  if (!out.begin(cfg_out)) stop();

  // format changes in vban must change the output as well
  in.addNotifyAudioChange(out);

  // setup input from vban
  auto cfg_in = in.defaultConfig(RX_MODE);
  cfg_in.ssid = "ssid";
  cfg_in.password = "password";
  cfg_in.stream_name = "Talkie";

  // set parameters for reply to discovery VBAN PING0 packet (So the voicemeter can correctly identify your device)
  //cfg_in.device_flags = 0x00000001; //reciever only
  //cfg_in.bitfeature = 0x00000001;  // audio only
  //cfg_in.device_color = 0x00FF00;
  //cfg_in.device_name = nullptr; // or "MyDeviceName" if empty replaced with MAC 
  //cfg_in.manufacturer_name = "Your Name";
  //cfg_in.application_name = "Your App Name";
  //cfg_in.host_name = nullptr; // if empty replaced with hostname
  //cfg_in.user_name = "User";
  //cfg_in.user_comment = "ESP32 VBAN Audio Device"; //bascially anything goes - just keep under 64 bytes see VBAN docs for more

  in.begin(cfg_in);
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}