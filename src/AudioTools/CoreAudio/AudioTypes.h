#pragma once

#include "AudioConfig.h"
#ifdef USE_TYPETRAITS
#  include <type_traits>
#  include <limits>
#endif

#include "AudioTools/CoreAudio/AudioLogger.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"

// fix compile error for ESP32 C3
#undef HZ

// MIN
#undef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))


namespace audio_tools {

using sample_rate_t = uint32_t;

/**
 * @brief The Microcontroller is the Audio Source (TX_MODE) or Audio Sink (RX_MODE). RXTX_MODE is Source and Sink at the same time!
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
static const char* RxTxModeNames[4]={"UNDEFINED_MODE","TX_MODE","RX_MODE","RXTX_MODE" };
/**
 * @brief Time Units
 * @ingroup basic
 */
enum TimeUnit {MS, US, HZ};
static const char* TimeUnitStr[3] {"MS","US","HZ"};

/**
 * @brief Basic Audio information which drives e.g. I2S
 * @ingroup basic
 */
struct AudioInfo {

    /// Sample Rate: e.g 44100
    sample_rate_t sample_rate = DEFAULT_SAMPLE_RATE;    
    /// Number of channels: 2=stereo, 1=mono
    uint16_t channels = DEFAULT_CHANNELS;  
    /// Number of bits per sample (int16_t = 16 bits)    
    uint8_t bits_per_sample = DEFAULT_BITS_PER_SAMPLE; 

    /// Default constructor
    AudioInfo() = default;

    /// Constructor which supports all attribures as parameters
    AudioInfo(sample_rate_t sampleRate, uint16_t channelCount, uint8_t bitsPerSample) {
        sample_rate = sampleRate;
        channels = channelCount;
        bits_per_sample = bitsPerSample;
    };

    /// Copy constructor
    AudioInfo(const AudioInfo &) = default;
    
    /// Returns true if alt values are the same like the current values
    bool operator==(AudioInfo alt){
        return sample_rate==alt.sample_rate && channels == alt.channels && bits_per_sample == alt.bits_per_sample;
    }

    /// Returns true if alt values are the different from the current values
    bool operator!=(AudioInfo alt){
        return !(*this == alt);
    } 

    /// Returns true if alt values are the same like the current values
    bool equals(AudioInfo alt){
        return *this == alt;
    }

    /// Checks if only the sample rate is different
    bool equalsExSampleRate(AudioInfo alt){
        return channels == alt.channels && bits_per_sample == alt.bits_per_sample;
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

#ifndef SWIG
    /// Same as set
    AudioInfo& operator= (const AudioInfo& info){
      setAudioInfo(info);
      return *this;      
    }
#endif
    /// Returns true if all components are defined (no component is 0)
    operator bool() {
      return sample_rate > 0 && channels > 0 && bits_per_sample > 0;
    }

    virtual void clear(){
      sample_rate = 0;
      channels = 0;
      bits_per_sample = 0;
    }

    virtual void logInfo(const char* source="") {
        LOGI("%s sample_rate: %d / channels: %d / bits_per_sample: %d",source, (int) sample_rate, (int)channels, (int)bits_per_sample);
    }  

};

/**
 * @brief Supports changes to the sampling rate, bits and channels
 * @ingroup basic
 */
class AudioInfoSupport {
    public:
      /// Defines the input AudioInfo
      virtual void setAudioInfo(AudioInfo info) = 0;
      /// provides the actual input AudioInfo
      virtual AudioInfo audioInfo() = 0;
      /// provides the actual output AudioInfo: this is usually the same as audioInfo() unless we use a transforming stream
      virtual AudioInfo audioInfoOut() { return audioInfo();}
};
// Support legacy name
#if USE_OBSOLETE
using AudioBaseInfo = AudioInfo;
using AudioBaseInfoDependent = AudioInfoSupport;
using AudioInfoDependent = AudioInfoSupport;
#endif

/**
 * @brief Supports the subscription to audio change notifications
 * @ingroup basic
 */
class AudioInfoSource {
    public:
      /// Adds target to be notified about audio changes
      virtual void addNotifyAudioChange(AudioInfoSupport &bi) {
        if (!notify_vector.contains(&bi)) notify_vector.push_back(&bi);
      }

      /// Removes a target in order not to be notified about audio changes
      virtual bool removeNotifyAudioChange(AudioInfoSupport &bi){
        int pos = notify_vector.indexOf(&bi);
        if (pos<0) return false;
        notify_vector.erase(pos);
        return true;
      }

      /// Deletes all change notify subscriptions
      virtual void clearNotifyAudioChange() {
        notify_vector.clear();
      }

      /// Deactivate/Reactivate automatic AudioInfo updates: (default is active)
      void setNotifyActive(bool flag){
        is_notify_active = flag;
      }

      /// Checks if the automatic AudioInfo update is active 
      bool isNotifyActive(){
        return is_notify_active;
      }

    protected:
      Vector<AudioInfoSupport*> notify_vector;
      bool is_notify_active = true;

      void notifyAudioChange(AudioInfo info){
        if (isNotifyActive()){
          for(auto n : notify_vector){
              n->setAudioInfo(info);
          }
        }
      }

};

/**
 * @brief Supports the setting and getting of the volume
 * @ingroup volume
 */
class VolumeSupport {
  public:
    /// provides the actual volume in the range of 0.0f to 1.0f
    virtual float volume() { return volume_value; }
    /// define the actual volume in the range of 0.0f to 1.0f
    virtual bool setVolume(float volume) { 
      volume_value = volume; 
      return true; 
    }
  protected:
    float volume_value = 1.0f;
};

/**
 * @brief E.g. used by Encoders and Decoders
 * @ingroup basic
 */
class AudioWriter : public AudioInfoSupport {
    public: 
        virtual size_t write(const uint8_t *data, size_t len) = 0;
        virtual void setAudioInfo(AudioInfo from) = 0;
        virtual void setOutput(Print &out_stream) = 0;
        virtual operator bool() = 0;
        virtual bool begin() = 0;
        virtual bool begin(AudioInfo info) {
            setAudioInfo(info);
            return begin();
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

        /// converts milliseconds to bytes
        static uint32_t toBytes(uint32_t millis, AudioInfo info){
            size_t samples = info.sample_rate * millis / 1000;
            size_t bytes = samples * info.channels * info.bits_per_sample / 8;
            return bytes;
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

        static float toRateUs(uint32_t time_us){
            return 1000000.0 / time_us;
        }
        
        static float toRateMs(uint32_t time_ms){
            return 1000.0 / time_ms;
        }
};

/// @brief  Similar to Arduino map function but using floats
/// @param x 
/// @param in_min 
/// @param in_max 
/// @param out_min 
/// @param out_max 
/// @return 
template <typename T> 
inline T mapT(T x, T in_min, T in_max, T out_min, T out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}



/**
 * @brief Converts from a source to a target number with a different type
 * @ingroup basic
 */
class NumberConverter {
    public:
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
        static float maxValueT(){
#ifdef USE_TYPETRAITS
            // int24_t uses 4 bytes instead of 3!
            //return (std::is_same<T, int24_t>::value ) ? 8388607 : maxValue(sizeof(T)*8);
            return std::numeric_limits<T>::max();
#else 
            return maxValue(sizeof(T)*8);
#endif
        }

        template <typename T> 
        static float minValueT(){
#ifdef USE_TYPETRAITS
            // int24_t uses 4 bytes instead of 3!
            //return (std::is_same<T, int24_t>::value ) ? 8388607 : maxValue(sizeof(T)*8);
            return std::numeric_limits<T>::min();
#else 
            return -maxValue(sizeof(T)*8);
#endif
        }



        /// Clips the value to avoid any over or underflows
        template <typename T> 
        static T clipT(float value){
            float mv = maxValueT<T>();
            if (value > mv){
                return mv;
            } else if (value < -mv){
                return -mv;
            } 
            return value;
        }

        /// clips a value
        inline static int32_t clip(float value, int bits){
            float mv = maxValue(bits);
            if (value > mv){
                return mv;
            } else if (value < -mv){
                return -mv;
            } 
            return value;
        }

        /// Convert an integer integer autio type to a float (with max 1.0)
        template <typename T> 
        static float toFloatT(T value) {
          return static_cast<float>(value) / maxValueT<T>();
        }

        /// Convert a float (with max 1.0) to an integer autio type
        template <typename T> 
        static T fromFloatT(float value){
          return value * maxValueT<T>();
        }

        /// Convert an integer integer autio type to a float (with max 1.0)
        inline static float toFloat(int32_t value, int bits) {
          return static_cast<float>(value) / maxValue(bits);
        }

        /// Convert a float (with max 1.0) to an integer autio type
        inline static int32_t fromFloat(float value, int bits){
          return clip(value * maxValue(bits), bits);
        }

        /// Convert an int number from one type to another
        template <typename FromT, typename ToT> 
        static ToT convert(FromT value){
            float value1 = value;
            float minTo = minValueT<ToT>();
            float maxTo = maxValueT<ToT>();
            float maxFrom = maxValueT<FromT>();
            float minFrom = minValueT<FromT>();

            if (maxTo - minTo > 1.0f
            || maxFrom - minFrom > 1.0f) {
              return mapT<float>(value1, minFrom, maxFrom, minTo, maxTo);
            }
            

            return value1 * maxValueT<ToT>() / maxValueT<FromT>();
        }

        /// Convert an array of int types
        template <typename FromT, typename ToT> 
        static void convertArray(FromT* from, ToT*to, int samples, float vol=1.0f){
            float  factor = static_cast<float>(maxValueT<ToT>()) / maxValueT<FromT>();
            float vol_factor = factor * vol;
            for (int j=0;j<samples;j++){
              to[j] = clipT<ToT>(vol * convert<FromT, ToT>(from[j]));
            }
        }

};

#if defined(USE_I2S) 

/**
 * @brief I2S Formats
 */
enum I2SFormat {
  I2S_STD_FORMAT,
  I2S_LSB_FORMAT,
  I2S_MSB_FORMAT,
  I2S_PHILIPS_FORMAT,
  I2S_RIGHT_JUSTIFIED_FORMAT,
  I2S_LEFT_JUSTIFIED_FORMAT,
  I2S_PCM,
};

static const char* i2s_formats[] = {"I2S_STD_FORMAT","I2S_LSB_FORMAT","I2S_MSB_FORMAT","I2S_PHILIPS_FORMAT","I2S_RIGHT_JUSTIFIED_FORMAT","I2S_LEFT_JUSTIFIED_FORMAT","I2S_PCM"};

/**
 * @brief I2S Signal Types: Digital, Analog, PDM
 */
enum I2SSignalType {
  Digital,
  Analog,
  PDM,
  TDM,
};

static const char* i2s_signal_types[] = {"Digital","Analog","PDM","TDM"};

#endif

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
size_t readSamples(Stream* p_stream, T* data, int samples, int retryCount=-1){
  uint8_t *p_result = (uint8_t*) data;
  int open = samples*sizeof(T);
  int total = 0;
  int count0 = 0;
  // copy missing data - we abort after 5 x read returned 0 bytes
  while (open > 0){
    int read = p_stream->readBytes(p_result+total, open);
    open -= read;
    total += read;
    if (read == 0){
      count0++;
      delay(1);
    } else {
      count0 = 0;
    }
    // abort loop
    if (retryCount >= 0 && count0 > retryCount)
      break;
  }
  // convert bytes to samples
  return total / sizeof(T);
}

/// guaranteed to return the requested data
template<typename T>
size_t  writeSamples(Print* p_out, T* data, int samples, int maxSamples=512){
  uint8_t *p_result = (uint8_t*) data;
  int open = samples*sizeof(T);
  int total = 0;
  // copy missing data
  while (open>0){
    int to_write = min(open, static_cast<int>(maxSamples*sizeof(T)));
    int written = p_out->write(p_result+total, to_write );
    open -= written;
    total += written;
  }
  // convert bytes to samples
  return total / sizeof(T);
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