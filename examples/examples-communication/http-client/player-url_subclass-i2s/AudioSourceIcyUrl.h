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

        /// Returns the last section of a url: https://22323.live.streamtheworld.com/TOPRETRO.mp3 gives TOPRETRO.mp3
        const char *urlName(){
            const char* result = "";
            StrView tmpStr(toStr());
            int pos = tmpStr.lastIndexOf("/");
            if (pos>0){
                result = toStr()+pos+1;
            } 
            return result;
        }

        /// Returns the icy name if available otherwise we use our custom logic
        const char* name() {
            StrView result(icyName());
            if (result.isEmpty()){
                result.set(urlName());
            }
            return result.c_str();
        }
};

}