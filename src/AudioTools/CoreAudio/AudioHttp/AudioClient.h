#pragma once
#ifdef ARDUINO
#  include "Client.h"
#else
// e.g. IDF does not know about the Arduino Client
#  include "Stream.h"
using Client = Stream;
#endif