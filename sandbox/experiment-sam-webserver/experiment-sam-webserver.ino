/**
 * @file experiment-sam-webserver.ino
 * @author Phil Schatzmann
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "AudioTools.h"
#include "AudioWAVServer.h"
#include "sam_arduino.h"

using namespace audio_tools;  

extern const char *alice;
AudioWAVServer server("network", "password");
int channels = 1;
int audio_rate = 22050; 


// Callback which provides the audio data 
void outputData(Stream &out){
  Serial.println("providing audio data...");
  SAM sam(out, false, channels);
  sam.say(alice);
}

void setup(){
  Serial.begin(115200);

  // start data sink
  server.begin(outputData, audio_rate, channels);
}


// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}