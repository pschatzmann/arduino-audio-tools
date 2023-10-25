/**
 * @file send-receive.ino
 * @author Phil Schatzmann
 * @brief Receive audio sent by snapcast on an ESP32 and output to i2s.
 * I was testing with ffmpeg -i http://stream.srg-ssr.ch/m/rsj/mp3_128 -f s16le -ar 48000  /tmp/snapfifo
 * More examples can be found at https://github.com/pschatzmann/arduino-snapclient/tree/main/examples
 * @version 0.1
 * @date 2023-09-25
 *
 * @copyright Copyright (c) 2022
 */

#include "AudioTools.h"
#include "SnapClient.h" // install https://github.com/pschatzmann/arduino-snapclient

I2SStream out;
SnapClient client(out);

void setup() {
  // login to wifi
  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }

  // print ip address
  Serial.println();
  Serial.println(WiFi.localIP());

  // setup I2S to define custom pins
  auto cfg = out.defaultConfig();
  config.pin_bck = 14;
  config.pin_ws = 15;
  config.pin_data = 22;
  out.begin(cfg);

  // start snap client
  client.begin();
}

void loop() {
  delay(100);
}