#pragma once
#include "AudioToolsConfig.h"

// Different platforms have different WiFi libraries

#if defined(USE_WIFININA)
#  include <WiFiNINA.h>
#elif defined(USE_WIFIS3)
#  include <WiFiS3.h>
#elif defined(ESP8266)
#  include <ESP8266WiFi.h>
#elif defined(ESP32)
#  include <Client.h>
#  include <WiFi.h>
#  include <WiFiClientSecure.h>
#  include <esp_wifi.h>
#else
#  include <WiFi.h>
#endif

