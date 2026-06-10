#pragma once
#include "AudioToolsConfig.h"

#if defined(IS_ZEPHYR)
#  include "WiFiZephyr.h"
#  include "WiFiClientZephyr.h"
#  include "WiFiClientSecureZephyr.h"
#  include "WiFiServerZephyr.h"
#elif defined(USE_WIFININA)
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
