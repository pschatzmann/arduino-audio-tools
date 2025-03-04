#pragma once
#include <stdlib.h>
#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioHttp/HttpRequest.h"
#include "AudioTools/CoreAudio/AudioMetaData/MetaDataFilter.h"
#include "MetaDataICY.h"
#include "MetaDataID3.h"

/** 
 * @defgroup metadata Metadata
 * @ingroup main
 * @brief Audio Metadata (Title, Author...)
**/

namespace audio_tools {

/**
 * @brief ID3 and Icecast/Shoutcast metadata output support. Just write the audio data 
 * to an object of this class and receive the metadata via the callback.
 * @ingroup metadata
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataOutput : public AudioOutput {
  public:

    MetaDataOutput() = default;
    MetaDataOutput(MetaDataOutput const&) = delete;
    MetaDataOutput& operator=(MetaDataOutput const&) = delete;

    ~MetaDataOutput(){
        end();
        if (meta!=nullptr) delete meta;
    }

    /// Defines the callback
    virtual void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
        TRACED();
        callback = fn; 
    }

#ifdef USE_URL_ARDUINO

    /// Starts the processing - iceMetaint is determined from the HttpRequest
    virtual void begin(AbstractURLStream &url) {
        TRACED();
        ICYUrlSetup icySetup;
        int metaInt = icySetup.setup(url);
        icySetup.executeCallback(callback);
        begin(metaInt);
    }
#endif

    /// Starts the processing - if iceMetaint is defined we use icecast
    virtual void begin(int iceMetaint=0) {
        LOGD("%s: %d", LOG_METHOD, iceMetaint);
        if (callback!=nullptr){
            if (meta == nullptr) {
#if defined(USE_URL_ARDUINO)
                meta = (iceMetaint > 0) ? new MetaDataICY() : (AbstractMetaData *)  new MetaDataID3();
#else
                meta =  new MetaDataID3();
#endif
            }
            meta->setCallback(callback);    
            meta->setIcyMetaInt(iceMetaint);
            meta->begin();
        } else {
            LOGI("callback not defined -> not Metadata processing")
        }
    }

    virtual void end() {
        if (callback!=nullptr && meta != nullptr) {
            TRACED();
            meta->end();
        }
    }

    /// Provide tha audio data to the API to parse for Meta Data
    virtual size_t write(const uint8_t *data, size_t len){
        LOGD("%s: %d", LOG_METHOD, (int)len);

        if (callback!=nullptr){
            if (meta!=nullptr){
                //CHECK_MEMORY();
                if (meta->write(data, len)!=len){
                    LOGE("Did not write all data");
                }
                //CHECK_MEMORY();
            } else {
                LOGW("meta is null");
            }
        }
        return len;
    }

     virtual size_t write(uint8_t c) {
         LOGE("Not Supported");
         return 0;
     }

  protected:
    AbstractMetaData *meta=nullptr;
    void (*callback)(MetaDataType info, const char* str, int len)=nullptr;

};

// legacy name
#if USE_OBSOLETE
using MetaDataPrint = MetaDataOutput;
#endif

}