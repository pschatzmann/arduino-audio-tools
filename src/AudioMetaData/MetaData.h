#pragma once

#include <stdlib.h>
#include "AudioTools/AudioTypes.h"
#include "AudioTools/AudioStreams.h"
#include "AudioMetaData/MetaDataICY.h"
#include "AudioMetaData/MetaDataID3.h"
#include "AudioHttp/Str.h"

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
        callback = fn; 
    }

    /// Starts the processing - iceMetaint is passed as string: if iceMetaint is defined we use icecast
    void begin(const char* iceMetaintStr) {
        int iceMetaint = 0;
        Str value(iceMetaintStr);
        if (!value.isEmpty()){
            iceMetaint = atoi(iceMetaintStr);
            LOGI("iceMetaint: %d", iceMetaint);
        }
        begin(iceMetaint);
    }

    /// Starts the processing - if iceMetaint is defined we use icecast
    void begin(int iceMetaint=0) {
        if (callback!=nullptr && meta != nullptr) {
            meta = iceMetaint>0 ? (MetaDataCommon *)  new MetaDataICY() : (MetaDataCommon *)  new MetaDataID3();
            meta->setCallback(callback);    
            meta->setIcyMetaInt(iceMetaint);
            meta->begin();
        }
    }

    void end() {
        if (callback!=nullptr & meta != nullptr) {
            meta->end();
        }
    }

    /// Provide tha audio data to the API to parse for Meta Data
    virtual size_t write(const uint8_t *data, size_t length){
        if (callback!=nullptr & meta != nullptr) {
            meta->write(data, length);
        }
        return length;
    }

     virtual size_t write(uint8_t) {
         LOGE("Not Supported");
         return 0;
     }

  protected:
    MetaDataCommon *meta;
    void (*callback)(MetaDataType info, const char* str, int len);

};


}