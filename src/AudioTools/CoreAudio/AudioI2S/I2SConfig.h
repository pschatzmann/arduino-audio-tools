#pragma once
#include "AudioToolsConfig.h"
#ifdef USE_I2S

#if defined(ESP32)
#  if USE_LEGACY_I2S
#    include "I2SConfigESP32.h"
#  else
#    include "I2SConfigESP32V1.h"
#  endif
#else
#  include "I2SConfigStd.h"
#endif

#endif