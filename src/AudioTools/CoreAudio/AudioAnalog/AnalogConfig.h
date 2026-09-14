#pragma once

// Select proper config for the specifc platform

#if defined(ESP32)
#  include "AnalogConfigESP32.h"
#  include "AnalogConfigESP32V1.h"
#elif defined(IS_ZEPHYR)
#  include "AnalogConfigZephyr.h"
#else
#  include "AnalogConfigStd.h"
#endif
