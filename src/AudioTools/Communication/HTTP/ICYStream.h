#pragma once
#include "AudioTools/Communication/HTTP/URLStream.h"
#include "AudioTools/Communication/HTTP/ICYStreamT.h"

namespace audio_tools {

/// Type alias for ICYStream
using ICYStream = ICYStreamT<URLStream>;

#if defined(USE_CONCURRENCY)
/// Type alias for buffered ICYStream
using ICYStreamBuffered = URLStreamBufferedT<ICYStream>;

#endif

}