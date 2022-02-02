#include "AudioTools.h"
#include "AudioServer.h"



AudioServer server("ssid","password");
UrlStream url;
StreamCopy copier; 

// Callback which provides the audio data to Server
void outputData(Stream &out){
  Serial.println("providing data...");

  // get sound data from Rhasspy 
  url.begin("http://192.168.1.37:12101/api/text-to-speech", UrlStream::POST, "text/plain","Hallo, my name is Alice");
  size_t byte_count = url.available();
  Serial.print("No of bytes received: ");
  Serial.println(byte_count);

  copier.begin(out, url);
  copier.copyAll();
  
  Serial.println("Copy done");

}

void setup(){
  Serial.begin(115200);
  // start server
  server.begin(outputData, "audio/wav");  
}


// Arduino loop  
void loop() {
  // Handle new connections
  server.doLoop();  
}
