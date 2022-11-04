#pragma once

#include "AudioConfig.h"
#include "AudioBasic/Int24.h"
#include "AudioBasic/Collections.h"

namespace audio_tools {

/**
 * @brief Audio Source (TX_MODE) or Audio Sink (RX_MODE)
 */
enum RxTxMode  {UNDEFINED_MODE=0, TX_MODE=1, RX_MODE=2, RXTX_MODE=3 };
INLINE_VAR const char* RxTxModeNames[]={"UNDEFINED_MODE","TX_MODE","RX_MODE","RXTX_MODE" };
/**
 * @brief Time Units
 */

enum TimeUnit {MS, US};

/**
 * @brief Basic Audio information which drives e.g. I2S
 * 
 */
struct AudioBaseInfo {
    AudioBaseInfo() = default;
    AudioBaseInfo(const AudioBaseInfo &) = default;
    int sample_rate = 0;    // undefined
    int channels = 0;       // undefined
    int bits_per_sample=16; // we assume int16_t

    bool operator==(AudioBaseInfo alt){
        return sample_rate==alt.sample_rate && channels == alt.channels && bits_per_sample == alt.bits_per_sample;
    }
    bool operator!=(AudioBaseInfo alt){
        return !(*this == alt);
    } 
    
    void setAudioInfo(AudioBaseInfo info){
      sample_rate = info.sample_rate;
      channels = info.channels;
      bits_per_sample = info.bits_per_sample;
    }

    void copyFrom(AudioBaseInfo info){
      setAudioInfo(info);
    }

    AudioBaseInfo& operator= (const AudioBaseInfo& info){
      setAudioInfo(info);
      return *this;      
    }

    virtual void logInfo() {
      LOGI("sample_rate: %d", sample_rate);
      LOGI("channels: %d", channels);
      LOGI("bits_per_sample: %d", bits_per_sample);
    }      
};

/**
 * @brief Supports changes to the sampling rate, bits and channels
 */
class AudioBaseInfoDependent {
    public:
      virtual ~AudioBaseInfoDependent(){}
      virtual void setAudioInfo(AudioBaseInfo info)=0;
      virtual AudioBaseInfo audioInfo() = 0;
      virtual bool validate(AudioBaseInfo &info){
        return true;
      }
};

/**
 * @brief Supports the subscription to audio change notifications
 */
class AudioBaseInfoSource {
    public:
      virtual void  setNotifyAudioChange(AudioBaseInfoDependent &bi) = 0;
};


/**
 * @brief E.g. used by Encoders and Decoders
 * 
 */
class AudioWriter {
  public: 
      virtual size_t write(const void *in_ptr, size_t in_size) = 0;
      virtual void setAudioInfo(AudioBaseInfo from) = 0;
      virtual void setOutputStream(Print &out_stream) = 0;
      virtual operator bool() = 0;
      virtual void begin() = 0;
      virtual void end() = 0;
};


INLINE_VAR const char* mime_pcm = "audio/pcm";


#ifndef IS_DESKTOP
/// wait for the Output to be ready
inline void waitFor(HardwareSerial &out){
    while(!out);
}
#endif

/// wait for flag to be active
inline void waitFor(bool &flag){
    while(!flag);
}

/// Pins
using Pins = Vector<int>;


}