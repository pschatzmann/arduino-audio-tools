#pragma once

namespace audio_tools {

/**
 * @brief Abstract Base class for all URLStream implementations
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AbstractURLStream : public AudioStream  {
    public:
        // executes the URL request
        virtual bool begin(const char* urlStr, const char* acceptMime=nullptr, MethodID action=GET,  const char* reqMime="", const char*reqData="") = 0;
        // ends the request
        virtual void end()=0;
        /// provides access to the HttpRequest
        virtual HttpRequest &httpRequest()=0;
        // only the ICYStream supports this
        virtual bool setMetadataCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
            return false;
        }

};

}
