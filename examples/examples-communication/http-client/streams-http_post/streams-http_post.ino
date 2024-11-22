
/**
 * @file streams-url-post.ino
 * @author Phil Schatzmann
 * @brief example how to http post data from an input stream using the
 * HttpRequest class.
 * @copyright GPLv3
 */

#include "AudioTools.h"

const char *ssid = "your SSID";
const char *password = "your PASSWORD";
const char *url_str = "http://192.168.1.44:9988";
AudioInfo info(44100, 2, 16);
SineWaveGenerator<int16_t> sineWave(32000);
GeneratedSoundStream<int16_t> sound(sineWave);
TimedStream timed(sound);
WiFiClient client;
HttpRequest http(client);
StreamCopy copier(http, timed);  

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.println(WiFi.localIP());
}

// Arduino Setup
void setup(void) {
  // Open Serial
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  startWiFi();

  // Setup sine wave
  sineWave.begin(info, N_B4);

  // limit the size of the input stream to 60 seconds
  timed.setEndSec(60);
  timed.begin();

  // start post
  Url url(url_str);
  http.header().put(TRANSFER_ENCODING, CHUNKED); // uncomment if chunked
  if (!http.processBegin(POST, url, "audio/pcm")){
    Serial.println("post failed");
    stop();
  }
}

// Arduino loop - copy sound to out
void loop() {
  // posting the data
  if (copier.copy() == 0) {
    // closing the post
    http.processEnd();
  }
}