/**
 * @brief A simple WebSocket server sketch using the
 * https://github.com/Links2004/arduinoWebSockets library to send out some
 * PCM audio data when some clients are connnected.
 * @author Phil Schatzmann
 */

#include <WebSocketsServer.h>  // https://github.com/Links2004/arduinoWebSockets
#include <WiFi.h>
#include <WiFiMulti.h>

#include "AudioTools.h"
#include "AudioTools/Communication/WebSocketOutput.h"

WiFiMulti WiFiMulti;
WebSocketsServer webSocket(81);
WebSocketOutput out(webSocket);

// audio
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
StreamCopy copier(out, sound);  // copies sound into i2s

void setup() {
  // Serial.begin(921600);
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // connect to wifi
  WiFiMulti.addAP("SSID", "passpasspass");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  // start server
  webSocket.begin();

  // start i2s
  sineWave.begin(info, N_B4);
}

void loop() {
  webSocket.loop();
  // generate audio only when we have any clients
  if (webSocket.connectedClients() > 0) copier.copy();
}