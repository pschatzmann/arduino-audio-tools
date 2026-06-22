#pragma once
#include "AudioToolsConfig.h"
#if defined(USE_ZEPHYR_LOGGER)
#  include "AudioTools/CoreAudio/AudioLogger/AudioLoggerZephyr.h"
#elif defined(USE_IDF_LOGGER)
#  include "AudioTools/CoreAudio/AudioLogger/AudioLoggerIDF.h"
#else
#  include "AudioTools/CoreAudio/AudioLogger/AudioLoggerSTD.h"
#endif