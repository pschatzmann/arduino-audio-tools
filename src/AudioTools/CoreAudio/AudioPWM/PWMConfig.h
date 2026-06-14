#pragma once

#ifdef __AVR__
#  include "PWMConfigAVR.h"
#elif defined(IS_ZEPHYR)
#  include "PWMConfigZephyr.h"
#else
#  include "PWMConfigSTD.h"
#endif
