#pragma once

#include "AudioConfig.h"
#include "AudioTools/Int24.h"

namespace audio_tools {

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

    void logInfo() {
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
      virtual bool validate(AudioBaseInfo &info){
        return true;
      }
};


enum RxTxMode  { TX_MODE, RX_MODE };


enum I2SMode {
  I2S_STD_MODE,
  I2S_LSB_MODE,
  I2S_MSB_MODE,
  I2S_PHILIPS_MODE,
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE
};


/**
 * @brief E.g. used by Encoders and Decoders
 * 
 */
class AudioWriter {
  public: 
	    virtual size_t write(const void *in_ptr, size_t in_size) = 0;
      virtual operator boolean() = 0;
};

/**
 * @brief Docoding of encoded audio into PCM data
 * 
 */
class AudioDecoder : public AudioWriter {
  public: 
      AudioDecoder() = default;
      virtual ~AudioDecoder() = default;
  		virtual void setOutputStream(Print &out_stream) = 0;
      virtual void begin() = 0;
      virtual void end() = 0;
      virtual AudioBaseInfo audioInfo() = 0;
      virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) = 0;
};

/**
 * @brief  Encoding of PCM data
 * 
 */
class AudioEncoder : public AudioWriter {
  public: 
      AudioEncoder() = default;
      virtual ~AudioEncoder() = default;
  		virtual void setOutputStream(Print &out_stream) = 0;
      virtual void setAudioInfo(AudioBaseInfo info) = 0;
      virtual void begin() = 0;
      virtual void end() = 0;
      virtual const char *mime() = 0;
};


/**
 * @brief Dummpy no implmentation Codec. This is used so that we can initialize some pointers to decoders and encoders to make
 * sure that they do not point to null.
 */
class CodecNOP : public  AudioDecoder, public AudioEncoder {
    public:
        static CodecNOP *instance() {
            static CodecNOP self;
            return &self;
        } 

        virtual void begin() {}
        virtual void end() {}
  	    virtual void setOutputStream(Print &out_stream) {}
        virtual void setNotifyAudioChange(AudioBaseInfoDependent &bi) {}
        virtual void setAudioInfo(AudioBaseInfo info) {}

        virtual AudioBaseInfo audioInfo() {
            AudioBaseInfo info;
            return info;
        }
        virtual operator boolean() {
            return false;
        }
        virtual int readStream(Stream &in) { 
            return 0; 
        };

	      virtual size_t write(const void *in_ptr, size_t in_size) {
            return 0;           
        }
        virtual const char *mime() {
            return nullptr;
        }

};

/// stops any further processing by spinning in an endless loop
void stop() {
  #ifdef EXIT_ON_STOP
  exit(0);
  #else
  while(true){
    delay(1000);
  }
  #endif
}

}