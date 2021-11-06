#pragma once
#include "AudioTools.h"

namespace audio_tools {

/**
 * @brief URLStream which provides the ICY Http Parameters
 * 
 */
class AudioSourceIcyUrl : public AudioSourceURL {
    public:
        template<typename T, size_t N>
        AudioSourceIcyUrl(URLStream& urlStream, T(&urlArray)[N], const char* mime, int start=0)
        : AudioSourceURL(urlStream, urlArray, mime,start) {
        }

        const char *icyValue(const char* name) {
            return actual_stream->httpRequest().reply().get(name);
        }
        const char *icyName() {
            return icyValue("icy-name");
        }
        const char *icyDescription() {
            return icyValue("icy-description");
        }
        const char *icyGenre() {
            return icyValue("icy-genre");
        }

};

}