#pragma once
#include "AudioToolsConfig.h"
#if defined(IS_ZEPHYR)
#  include "AudioTools/CoreAudio/AudioLoggerZephyr.h"
#elif defined(USE_IDF_LOGGER)
#  include "AudioTools/CoreAudio/AudioLoggerIDF.h"
#else
#  include "AudioTools/CoreAudio/AudioLoggerSTD.h"
#endif