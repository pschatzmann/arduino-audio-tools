#pragma once

#ifdef ESP32
#  include "AudioBLEServer.h"
#  include "AudioBLEClient.h"
#else
#  error Device not supported
#endif

