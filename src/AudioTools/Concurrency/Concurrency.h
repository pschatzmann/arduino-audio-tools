#pragma once

#if defined(ESP32)
#include "RTOS.h"
#elif defined(RP2040)
#include "RP2040.h"
#elif defined(IS_ZEPHYR)
#include "Zephyr.h"
#endif