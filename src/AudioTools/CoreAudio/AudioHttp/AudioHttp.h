#pragma once
/** 
 * @defgroup http Http
 * @ingroup communications
 * @brief Http client & server  
**/

#include "URLStream.h"
#include "URLStreamBuffered.h"
#include "AudioServer.h"
#include "ICYStream.h"
#include "ICYStreamBuffered.h"
#ifdef ESP32
#  include "URLStreamESP32.h"
#endif