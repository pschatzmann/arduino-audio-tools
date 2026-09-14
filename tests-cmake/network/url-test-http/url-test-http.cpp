#include "AudioTools.h"
#include "AudioTools/Communication/AudioHttp.h"

WiFiClient client;
HttpRequest http(client);

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  Url url("http://detectportal.firefox.com");

  int statusCode = http.get(url, "text/html");
  Serial.print("Status: ");
  Serial.println(statusCode);
  if (statusCode != 200) {
    stop();
  }
  Serial.println("returned from get");

  uint8_t buffer[1024];

  while (http.available()) {
    int bytesRead = http.readBytes(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      buffer[bytesRead] = 0;
      Serial.print((char*)buffer);
    } else {
      break;
    }
  }
  http.end();
  Serial.println("\nEND");
}

void loop() {
  delay(1000);
  Serial.println("ping...");
}
