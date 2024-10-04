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

/**
 * @brief SnapClient with Opus decoder: I2S OUtput 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#include "AudioTools.h"
#include "SnapClient.h"
#include "AudioTools/AudioCodecs/CodecOpus.h"

#define ARDUINO_LOOP_STACK_SIZE (10 * 1024)

OpusAudioDecoder opus;
I2SStream out;
WiFiClient wifi;
SnapTimeSyncDynamic synch(172, 10); // optional configuratioin
SnapClient client(wifi, out, opus);

void setup() {
  Serial.begin(115200);

  // login to wifi -> Define values in SnapConfig.h or replace them here
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
  cfg.pin_bck = 14;
  cfg.pin_ws = 15;
  cfg.pin_data = 22;
  //cfg.buffer_size = 512;
  //cfg.buffer_count = 6;
  out.begin(cfg);

  // Define CONFIG_SNAPCAST_SERVER_HOST in SnapConfig.h or here
  // client.setServerIP(IPAddress(192,168,1,38));

  // start snap client
  client.begin(synch);
}

void loop() {
  client.doLoop();
}