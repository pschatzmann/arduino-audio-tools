/**
 * @brief A simple WebSocket server sketch that receives PCM audio data using the
 * https://github.com/Links2004/arduinoWebSockets library and outputs it via i2s
 * to an AudioKit for easy testing. Replace the output with whatever other class
 * you like.
 * @author Phil Schatzmann
 */
#include <WebSocketsServer.h>  //https://github.com/Links2004/arduinoWebSockets
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

WiFiMulti WiFiMulti;
WebSocketsServer webSocket(81);
AudioInfo info(44100, 2, 16);
AudioBoardStream i2s(AudioKitEs8388V1);  // Access I2S as stream

/// Just output the audio data
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload,
                    size_t length) {
  if (type == WStype_BIN) {
    i2s.write(payload, length);
  }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  WiFiMulti.addAP("SSID", "passpasspass");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }
  WiFi.setSleep(false);
  Serial.println(WiFi.localIP());

  // start server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // start i2s
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  i2s.begin(cfg);
}

void loop() { webSocket.loop(); }