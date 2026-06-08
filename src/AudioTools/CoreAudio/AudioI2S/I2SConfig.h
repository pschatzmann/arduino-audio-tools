#pragma once
#include "AudioToolsConfig.h"
#ifdef USE_I2S

#if defined(ESP32)
#  if USE_LEGACY_I2S
#    include "I2SConfigESP32.h"
#  else
#    include "I2SConfigESP32V1.h"
#  endif
#elif defined(IS_ZEPHYR) 
#  include "I2SConfigZephyr.h"
#else
#  include "I2SConfigSTD.h"
#endif

#endif