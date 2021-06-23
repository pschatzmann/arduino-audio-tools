/**
 * @file streams-sam-webserver_wav.ino
 * 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
#include "sam_arduino.h"
#include "AudioServer.h"

using namespace audio_tools;  

AudioWAVServer server("ssid","password");
int channels = 1;
int bits_per_sample = 8;

// Callback which provides the audio data 
void outputData(Stream &out){
  Serial.print("providing data...");
  SAM sam(out,  false);
  sam.setOutputChannels(channels);
  sam.setBitsPerSample(bits_per_sample);

  sam.say("hallo, I am SAM");
}

void setup(){
  Serial.begin(115200);
  // start data sink
  server.begin(outputData, SAM::sampleRate(), channels, bits_per_sample);
}


// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}