#pragma once

#include <stdlib.h>
#include "AudioTools/AudioTypes.h"
#include "AudioTools/AudioStreams.h"
#include "AudioMetaData/MetaDataICY.h"
#include "AudioMetaData/MetaDataID3.h"
#include "AudioHttp/HttpRequest.h"

namespace audio_tools {

/**
 * @brief ID3 and Icecast/Shoutcast metadata output support
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MetaDataPrint : public Print {
  public:

    MetaDataPrint() = default;

    ~MetaDataPrint(){
        end();
        if (meta!=nullptr) delete meta;
    }

    /// Defines the callback
    void setCallback(void (*fn)(MetaDataType info, const char* str, int len)) {
        LOGD(LOG_METHOD);
        callback = fn; 
    }

    /// Starts the processing - iceMetaint is determined from the HttpRequest
    void begin(HttpRequest &http) {
        const char* iceMetaintStr = http.reply().get("icy-metaint");
        Str value(iceMetaintStr);
        int iceMetaint = value.toInt();
        LOGI("iceMetaint: %d", iceMetaint);

        begin(iceMetaint);

        // Callbacks filled from url reply for icy
        if (callback!=nullptr) {
            // handle icy parameters
            Str genre(http.reply().get("icy-genre"));
            if (!genre.isEmpty()){
                callback(Genre, genre.c_str(), genre.length());
            }

            Str descr(http.reply().get("icy-description"));
            if (!descr.isEmpty()){
                callback(Description, descr.c_str(), descr.length());
            }

            Str name(http.reply().get("icy-name"));
            if (!name.isEmpty()){
                callback(Name, name.c_str(), name.length());
            }
        }

    }

    /// Starts the processing - if iceMetaint is defined we use icecast
    void begin(int iceMetaint=0) {
        LOGD("%s: %d", LOG_METHOD, iceMetaint);
        if (callback!=nullptr){
            if (meta == nullptr) {
                meta = (iceMetaint > 0) ? new MetaDataICY() : (MetaDataCommon *)  new MetaDataID3();
            }
            meta->setCallback(callback);    
            meta->setIcyMetaInt(iceMetaint);
            meta->begin();
        } else {
            LOGI("callback not defined -> not Metadata processing")
        }
    }

    void end() {
        if (callback!=nullptr & meta != nullptr) {
            LOGD(LOG_METHOD);
            meta->end();
        }
    }

    /// Provide tha audio data to the API to parse for Meta Data
    virtual size_t write(const uint8_t *data, size_t length){
        LOGD("%s: %d", LOG_METHOD, length);

        if (callback!=nullptr){
            if (meta!=nullptr){
                if (meta->write(data, length)!=length){
                    LOGE("Did not write all data");
                }
            } else {
                LOGW("meta is null");
            }
        }
        return length;
    }

     virtual size_t write(uint8_t c) {
         LOGE("Not Supported");
         return 0;
     }

  protected:
    MetaDataCommon *meta=nullptr;
    void (*callback)(MetaDataType info, const char* str, int len)=nullptr;

};


}