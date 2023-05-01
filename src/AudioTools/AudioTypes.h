#pragma once

#ifdef USE_TYPETRAITS
#  include <type_traits>
#endif
#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"
#include "AudioBasic/Int24.h"
#include "AudioBasic/Collections/Vector.h"

namespace audio_tools {

/**
 * @brief Audio Source (TX_MODE) or Audio Sink (RX_MODE). RXTX_MODE is Source and Sink at the same time!
 * @ingroup basic
 */
enum RxTxMode  {UNDEFINED_MODE=0, TX_MODE=1, RX_MODE=2, RXTX_MODE=3 };

/**
 * @brief Memory types
 * @ingroup basic
 */

enum MemoryType {RAM, PS_RAM, FLASH_RAM};

/**
 * @brief Text string (description) for RxTxMode
*/
INLINE_VAR const char* RxTxModeNames[]={"UNDEFINED_MODE","TX_MODE","RX_MODE","RXTX_MODE" };
/**
 * @brief Time Units
 * @ingroup basic
 */
enum TimeUnit {MS, US};

/**
 * @brief Basic Audio information which drives e.g. I2S
 * @ingroup basic
 */
struct AudioInfo {
    /// Default constructor
    AudioInfo() = default;

    /// Constructor which supports all attribures as parameters
    AudioInfo(int sampleRate, int channelCount, int bitsPerSample) {
        sample_rate = sampleRate;
        channels = channelCount;
        bits_per_sample = bitsPerSample;
    };

    /// Copy constructor
    AudioInfo(const AudioInfo &) = default;
    
    bool operator==(AudioInfo alt){
        return sample_rate==alt.sample_rate && channels == alt.channels && bits_per_sample == alt.bits_per_sample;
    }

    bool operator!=(AudioInfo alt){
        return !(*this == alt);
    } 
    
    /// Copies the values from info
    void set(AudioInfo info)  {
      sample_rate = info.sample_rate;
      channels = info.channels;
      bits_per_sample = info.bits_per_sample;
    }

    /// Same as set
    void setAudioInfo(AudioInfo info)  {
        set(info);
    }

    /// Same as set
    void copyFrom(AudioInfo info){
      setAudioInfo(info);
    }

    AudioInfo& operator= (const AudioInfo& info){
      setAudioInfo(info);
      return *this;      
    }

    virtual void logInfo() {
      LOGI("sample_rate: %d", sample_rate);
      LOGI("channels: %d", channels);
      LOGI("bits_per_sample: %d", bits_per_sample);
    }  

    // public attributes
    int sample_rate = 0;    // undefined
    int channels = 0;       // undefined
    int bits_per_sample=16; // we assume int16_t
    
};

/**
 * @brief Supports changes to the sampling rate, bits and channels
 * @ingroup basic
 */
class AudioInfoSupport {
    public:
      virtual void setAudioInfo(AudioInfo info)=0;
      virtual AudioInfo audioInfo() = 0;
      virtual bool validate(AudioInfo &info){
        return true;
      }
};

// Support legacy name
using AudioBaseInfo = AudioInfo;
using AudioBaseInfoDependent = AudioInfoSupport;
using AudioInfoDependent = AudioInfoSupport;


/**
 * @brief Supports the subscription to audio change notifications
 * @ingroup basic
 */
class AudioInfoSource {
    public:
      virtual void  setNotifyAudioChange(AudioInfoSupport &bi) = 0;
};


/**
 * @brief E.g. used by Encoders and Decoders
 * @ingroup basic
 */
class AudioWriter {
    public: 
        virtual size_t write(const void *in_ptr, size_t in_size) = 0;
        virtual void setAudioInfo(AudioInfo from) = 0;
        virtual void setOutputStream(Print &out_stream) = 0;
        virtual operator bool() = 0;
        virtual void begin() = 0;
        virtual void begin(AudioInfo info) {
            setAudioInfo(info);
            begin();
        }
        virtual void end() = 0;
    protected:
        void writeBlocking(Print *out, uint8_t* data, size_t len){
            TRACED();
            int open = len;
            int written = 0;
            while(open>0){
                int result = out->write(data+written, open);
                open -= result;
                written += result;
            }
        }
};

/**
 * @brief Tools for calculating timer values
 * @ingroup timer
 */
class AudioTime {
    public:
        /// converts sampling rate to delay in microseconds (μs)
        static uint32_t toTimeUs(uint32_t samplingRate, uint8_t limit=10){
            uint32_t result = 1000000l / samplingRate;
            if (1000000l % samplingRate!=0){
                result++;
            }
            if (result <= limit){
                LOGW("Time for samplingRate %u -> %u is < %u μs - we rounded up", (unsigned int)samplingRate,  (unsigned int)result,  (unsigned int)limit);
                result = limit;
            }
            return result;
        }

        static uint32_t toTimeMs(uint32_t samplingRate, uint8_t limit=1){
            uint32_t result = 1000l / samplingRate;
            if (1000000l % samplingRate!=0){
                result++;
            }
            if (result <= limit){
                LOGW("Time for samplingRate %u -> %u is < %u μs - we rounded up", (unsigned int)samplingRate,  (unsigned int)result,  (unsigned int)limit);
                result = limit;
            }
            return result;
        }
};

/**
 * @brief Converts from a source to a target number with a different type
 * @ingroup basic
 */
class NumberConverter {
    public:
        static int32_t convertFrom24To32(int24_t value)  {
            return value.scale32();
        }

        static int16_t convertFrom24To16(int24_t value)  {
            return value.scale16();
        }

        static float convertFrom24ToFloat(int24_t value)  {
            return value.scaleFloat();
        }

        static int16_t convertFrom32To16(int32_t value)  {
            return static_cast<float>(value) / INT32_MAX * INT16_MAX;
        }

        static int16_t convert16(int value, int value_bits_per_sample){
            return value * NumberConverter::maxValue(16) / NumberConverter::maxValue(value_bits_per_sample);
        }

        static int16_t convert8(int value, int value_bits_per_sample){
            return value * NumberConverter::maxValue(8) / NumberConverter::maxValue(value_bits_per_sample);
        }

        /// provides the biggest number for the indicated number of bits
        static int64_t maxValue(int value_bits_per_sample){
            switch(value_bits_per_sample){
                case 8:
                    return 127;
                case 16:
                    return 32767;
                case 24:
                    return 8388607;
                case 32:
                    return 2147483647;
            }
            return 32767;
        }

        /// provides the biggest number for the indicated type
        template <typename T> 
        static int64_t maxValueT(){
#ifdef USE_TYPETRAITS
            // int24_t uses 4 bytes instead of 3!
            return (std::is_same<T, int24_t>::value ) ? 8388607 : maxValue(sizeof(T)*8);
#else 
            return maxValue(sizeof(T)*8);
#endif
        }

        /// Clips the value to avoid any over or underflows
        template <typename T> 
        static T clip(int64_t value){
            T mv = maxValue(sizeof(T)*8);
            if (value > mv){
                return mv;
            } else if (value < -mv){
                return -mv;
            } 
            return value;
        }

        /// Convert a number from one type to another
        template <typename FromT, typename ToT> 
        static ToT convert(FromT value){
            int64_t value1 = value;
            return clip<ToT>(value1 * maxValueT<ToT>() / maxValueT<FromT>());
        }

};

/// guaranteed to return the requested data
template<typename T>
T readSample(Stream* p_stream){
  T result=0;
  uint8_t *p_result = (uint8_t*) &result;
  int open = sizeof(T);
  int total = 0;
  // copy missing data
  while (open>0){
    int read = p_stream->readBytes(p_result+total, open);
    open -= read;
    total += read;
  }
  return result;
}

/// guaranteed to return the requested data
template<typename T>
size_t  readSamples(Stream* p_stream, T* data, int samples){
  uint8_t *p_result = (uint8_t*) data;
  int open = samples*sizeof(T);
  int total = 0;
  // copy missing data
  while (open>0){
    int read = p_stream->readBytes(p_result+total, open);
    open -= read;
    total += read;
  }
  return samples;
}



/// @brief  Similar to Arduino map function but using floats
/// @param x 
/// @param in_min 
/// @param in_max 
/// @param out_min 
/// @param out_max 
/// @return 
inline float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/// @brief Mime type for PCM
static const char* mime_pcm = "audio/pcm";


#ifndef IS_DESKTOP
/// wait for the Output to be ready  
inline void waitFor(HardwareSerial &out){
    while(!out);
}
#endif

/// wait for flag to be active  @ingroup basic
inline void waitFor(bool &flag){
    while(!flag);
}

/// Pins  @ingroup basic
using Pins = Vector<int>;


}