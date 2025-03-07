#pragma once
#include "AudioConfig.h"
#if defined(USE_IDF_LOGGER)
#  include "AudioTools/CoreAudio/AudioLoggerIDF.h"
#else
#  include "AudioTools/CoreAudio/AudioLoggerSTD.h"
#endif