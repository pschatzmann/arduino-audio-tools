#pragma once
#ifdef ARDUINO
#  include "Client.h"
#else
// e.g. IDF does not know about the Arduino Client
#  include "AudioTools/AudioLibs/Desktop/NoArduino.h"
#endif

