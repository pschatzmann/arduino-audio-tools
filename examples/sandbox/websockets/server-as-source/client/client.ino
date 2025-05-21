/**
 * @brief A simple WebSocket client sketch that receives PCM audio data using the
 * https://github.com/Links2004/arduinoWebSockets library and outputs it via i2s
 * to an AudioKit for easy testing. Replace the output with whatever other class
 * you like.
 * @author Phil Schatzmann
 */
#include <WebSocketsClient.h>  // https://github.com/Links2004/arduinoWebSockets
#include <WiFiMulti.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

// websocket
WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
// audio
AudioInfo info(44100, 2, 16);
AudioBoardStream i2s(AudioKitEs8388V1);  // Access I2S as stream

// write audio to i2s
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_BIN) {
    i2s.write(payload, length);
  }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // connect to wifk
  WiFiMulti.addAP("SSID", "passpasspass");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  WiFi.setSleep(false);

  // start client connecting to server address, port and URL
  webSocket.begin("192.168.1.34", 81, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

  // start i2s
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  i2s.begin(cfg);
}

void loop() {
  webSocket.loop();
}