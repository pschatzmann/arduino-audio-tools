/**
 * Vosk WebSocket Streaming Example
 *
 * Start vosk server e.g. with 
 * docker run -d -p 2700:2700 alphacep/kaldi-vosk-server:latest 
 * 
 * Dependencies:
 *
 *
 **/
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* serverHost = "192.168.1.39";  // IP of Vosk server
const uint16_t serverPort = 2700;
WebSocketsClient webSocket;
AudioBoardStream i2s(AudioKitEs8388V1);  // Access I2S as stream
VolumeMeter meter(i2s);

// --- Audio activity callback ---
void audioActive(bool active) {
  Serial.println("Audio Active: " + String(active ? "true" : "false"));
  if (!active) {
    webSocket.sendTXT("{\"eof\" : 1}");
  }
}

// --- WebSocket event handler ---
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      // Send config so the server knows how to interpret the incoming raw PCM
      // (kaldi-vosk-server supports this JSON config format)
      webSocket.sendTXT("{\"config\":{\"sample_rate\":16000}}");
      break;
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      break;
    case WStype_TEXT:
      Serial.print("Vosk:");
      // payload is not guaranteed to be null-terminated
      Serial.write(payload, length);
      Serial.println();
      break;
    default:
      Serial.println("WebSocket event type: " + String(type));
      break;
  }
}

void sendAudioData() {
  uint8_t buffer[512];
  size_t bytesRead = meter.readBytes(buffer, sizeof(buffer));
  if (bytesRead > 0 && meter.isActive() && webSocket.isConnected()) {
    webSocket.sendBIN(buffer, bytesRead);
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize WebSocket
  webSocket.begin(serverHost, serverPort, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  // Initialize AudioTools I2S microphone
  auto cfg = i2s.defaultConfig(RX_MODE);
  cfg.sample_rate = 16000;  // Vosk prefers 16kHz
  cfg.bits_per_sample = 16;
  cfg.channels = 1;  // Mono
  cfg.input_device = ADC_INPUT_LINE2;
  i2s.begin(cfg);

  // Use volume meter to monitor audio activity
  meter.setActivityCallback(audioActive, 0.007, 2000);
  meter.begin(cfg);
}

void loop() {
  webSocket.loop();
  sendAudioData();
}
