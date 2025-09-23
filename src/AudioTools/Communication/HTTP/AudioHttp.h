#pragma once
/** 
 * @defgroup http Http
 * @ingroup communications
 * @brief Http client & server  
**/

// Include abstract base classes and utilities
#include "AbstractURLStream.h"
#include "HttpRequest.h"
#include "HttpHeader.h"
#include "HttpTypes.h"
#include "ICYStreamT.h"
#include "URLStreamBufferedT.h"
#include "Url.h"

// For backward compatibility, include stub implementations
#include "URLStream.h"
#include "ICYStream.h"
#include "AudioServer.h"

#if ((defined(ESP32) && defined(USE_URL_ARDUINO)) || defined(ESP32_CMAKE)) 
#  include "URLStreamESP32.h"
#endif

