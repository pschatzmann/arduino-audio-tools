#pragma once
/** 
 * @defgroup http Http
 * @ingroup communications
 * @brief Http client & server  
**/

#include "URLStream.h"
#include "AudioServer.h"
#if defined(ESP32) 
#  include "URLStreamESP32.h"
#endif