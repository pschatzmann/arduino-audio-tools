#pragma once
#if defined(ARDUINO) || defined(IS_DESKTOP)
#  include "Client.h"
#else
// e.g. IDF does not know about the Arduino Client
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#endif

