/**
 * @brief A simple WebSocket client sketch that sends out some PCM audio
 * data using the https://github.com/Links2004/arduinoWebSockets library
 * @author Phil Schatzmann
 */

#include <WebSocketsClient.h>  // https://github.com/Links2004/arduinoWebSockets
#include <WiFiMulti.h>

#include "AudioTools.h"
#include "AudioTools/Communication/WebSocketOutput.h"

// websocket
WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WebSocketOutput out(webSocket);
// audio
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
StreamCopy copier(out, sound);

void setup() {
  Serial.begin(115200);

  // connect to wifi
  WiFiMulti.addAP("SSID", "passpasspass");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  // start client connecting to server address, port and URL
  webSocket.begin("192.168.0.123", 81, "/");

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

  sineWave.begin(info, N_B4);
}

void loop() {
  webSocket.loop();
  // generate audio only when we are connected
  if (webSocket.isConnected()) copier.copy();
}