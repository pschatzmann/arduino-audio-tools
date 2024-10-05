#pragma once

#ifdef ESP32
//#  include "AudioBLEServer.h"
//#  include "AudioBLEClient.h"
 #  include "AudioBLEServerESP32.h"
 #  include "AudioBLEClientESP32.h"
#else
#  include "AudioBLEServer.h"
#  include "AudioBLEClient.h"
#endif

