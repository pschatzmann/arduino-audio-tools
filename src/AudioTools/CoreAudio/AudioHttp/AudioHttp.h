#pragma once
/** 
 * @defgroup http Http
 * @ingroup communications
 * @brief Http client & server  
**/

#include "URLStream.h"
#include "AudioServer.h"
#if defined(ESP32) && defined(USE_URL_ARDUINO) 
#  include "URLStreamESP32.h"
#endif