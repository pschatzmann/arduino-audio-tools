/**
 * @file streams-tts-webserver_wav.ino
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */

#include "AudioTools.h"
#include "TTS.h"

AudioWAVServer server("ssid","password");

// Callback which provides the audio data 
void outputData(Print *out){
  Serial.print("providing data...");
  TTS tts = TTS(*out);
  tts.sayText("Hallo, my name is Alice");
}

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);
  // start data sink
  TTSInfo info = TTS::getInfo();
  server.begin(outputData, info.sample_rate, info.channels, info.bits_per_sample);
}

// Arduino loop  
void loop() {
  // Handle new connections
  server.copy();  
}
