#pragma once
#include "AudioTools/Communication/HTTP/URLStream.h"
#include "AudioTools/Communication/HTTP/ICYStreamT.h"

namespace audio_tools {


/**
 * @brief Type alias for ICYStreamT<URLStream>.
 * @ingroup http
 */
using ICYStream = ICYStreamT<URLStream>;

#if defined(USE_CONCURRENCY)
/**
 * @brief Type alias for URLStreamBufferedT<ICYStream> (buffered ICYStream).
 * @ingroup http
 */
using ICYStreamBuffered = URLStreamBufferedT<ICYStream>;

#endif

}