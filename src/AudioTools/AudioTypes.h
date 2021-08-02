#pragma once

#include "AudioConfig.h"

namespace audio_tools {

#define INT24_MAX 0x7FFFFF

/**
 * @brief 24bit integer which is used for I2S sound processing. It works only on little endian machines!
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class int24_t : public Printable {

  public:

    int24_t(){
        value[0]=0;
        value[1]=0;
        value[2]=0;
    }

    int24_t(void *ptr){
        uint8_t *ptrChar = static_cast<uint8_t*>(ptr);
        value[0]=ptrChar[0];
        value[1]=ptrChar[1];
        value[2]=ptrChar[2];
    }

    int24_t(uint8_t *ptr){
        value[0]=ptr[0];
        value[1]=ptr[1];
        value[2]=ptr[2];
    }

    int24_t(const int16_t &in) {
      value[2] = in > 0 ? 0 : 0xFF;
      value[1] = (in >> 8) & 0xFF;
      value[0] = in & 0xFF;
    }

    int24_t(const int32_t &in){
      value[2] = (in >> 16) & 0xFF;;
      value[1] = (in >> 8) & 0xFF;
      value[0] = in & 0xFF;
    }

    operator int32_t() const {
        uint8_t tmp[4];
        tmp[0] = value[0];
        tmp[1] = value[1];
        tmp[2] = value[2];
        tmp[3] = value[2] & 0x80  ? 0xFF : 0;
        int32_t *ptr = (int32_t *)&tmp;
        return *ptr;
    }

    operator float() const {
        return static_cast<float>(*this);
    }

    /// provides value between -32767 and 32767
    int16_t scale16() const {
        return static_cast<float>(*this) * INT16_MAX / INT24_MAX ; 
    }

    /// provides value between -2,147,483,647 and 2,147,483,647
    int32_t scale32() const {
        return static_cast<float>(*this) / static_cast<float>(INT24_MAX) * static_cast<float>(INT32_MAX) ; 
    }

    /// provides value between -1.0 and 1.0
    float scaleFloat() const {
        return static_cast<float>(*this)  / INT24_MAX ; 
    }

    virtual size_t printTo(Print& p) const {
        // just print as int32_t
        const int32_t v = *this;
        return p.print(v);
    }

  private:
    uint8_t value[3]; 
};


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
};

/**
 * @brief Supports changes to the sampling rate, bits and channels
 */
class AudioBaseInfoDependent {
    public:
      virtual ~AudioBaseInfoDependent(){}
      virtual void setAudioInfo(AudioBaseInfo info) {};
      virtual bool validate(AudioBaseInfo &info){
        return true;
      }
};


enum RxTxMode  { TX_MODE, RX_MODE };


enum I2SMode {
  I2S_PHILIPS_MODE,
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE
};


#ifdef I2S_SUPPORT

/**
 * @brief configuration for all common i2s settings
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class I2SConfig : public AudioBaseInfo {
  public:

    I2SConfig() {
      channels = DEFAULT_CHANNELS;
      sample_rate = DEFAULT_SAMPLE_RATE; 
      bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    }

    /// Constructor
    I2SConfig(RxTxMode mode) {
        channels = DEFAULT_CHANNELS;
        sample_rate = DEFAULT_SAMPLE_RATE; 
        bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
        this->rx_tx_mode = mode;
        pin_data = rx_tx_mode == TX_MODE ? PIN_I2S_DATA_OUT : PIN_I2S_DATA_IN;
    }
    /// Default Copy Constructor
    I2SConfig(const I2SConfig &cfg) = default;


    /// public settings
    RxTxMode rx_tx_mode = TX_MODE;
    bool is_master = true;
    int port_no = 0;  // processor dependent port
    int pin_ws = PIN_I2S_WS;
    int pin_bck = PIN_I2S_BCK;
    int pin_data = PIN_I2S_DATA_OUT;
    I2SMode i2s_mode = I2S_PHILIPS_MODE;
    bool is_digital = true;  // e.g. the ESP32 supports analog input or output

};

#endif

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
  		virtual void setStream(Stream &out_stream) = 0;
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
  		virtual void setStream(Stream &out_stream) = 0;
      virtual void begin() = 0;
      virtual void end() = 0;
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