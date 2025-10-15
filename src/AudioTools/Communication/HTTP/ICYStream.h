#pragma once
#include "AudioTools/Communication/HTTP/ICYStreamT.h"
#include "AudioTools/Communication/HTTP/URLStream.h"

namespace audio_tools {

/**
 * @brief Type alias for ICYStreamT<URLStream>.
 * @ingroup http
 */
using ICYStream = ICYStreamT<URLStream>;

#if defined(USE_CONCURRENCY)
/**
 * @brief Buffered ICYStream with metadata callback support.
 * @ingroup http
 */
class ICYStreamBuffered : public URLStreamBufferedT<ICYStream> {
 public:
  using URLStreamBufferedT<ICYStream>::URLStreamBufferedT;

  /// Defines the metadata callback function
  virtual bool setMetadataCallback(void (*fn)(MetaDataType info,
                                              const char* str,
                                              int len)) override {
    this->urlStream.setMetadataCallback(fn);
    return true;
  }
};
#endif

}  // namespace audio_tools