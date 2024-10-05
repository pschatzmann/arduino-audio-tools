#pragma once
#include "AudioConfig.h"
#ifdef USE_I2S

#if defined(ESP32)
#  if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
#    include "I2SConfigESP32.h"
#  else
#    include "I2SConfigESP32V1.h"
#  endif
#else
#  include "I2SConfigStd.h"
#endif

#endif