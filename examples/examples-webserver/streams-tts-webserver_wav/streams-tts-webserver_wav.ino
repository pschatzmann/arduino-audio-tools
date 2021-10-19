/**
 * @file streams-tts-webserver_wav.ino
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "TTS.h"
#include "AudioTools.h"

using namespace audio_tools;  

AudioWAVServer server("ssid","password");

// Callback which provides the audio data 
void outputData(Stream &out){
  Serial.print("providing data...");
  TTS tts(out);
  tts.sayText("Hallo, my name is Alice");
}

void setup(){
  Serial.begin(115200);
  //AudioLogger::instance().begin(Serial, AudioLogger::Debug);
  // start data sink
  TTSInfo info = TTS::getInfo();
  server.begin(outputData, info.sample_rate, info.channels, info.bits_per_sample);
}


// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
