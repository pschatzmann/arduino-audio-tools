#include "AudioTools/CoreAudio/AudioStreams.h"

namespace audio_tools {

/**
 * @brief Determine Frequency using Audio Correlation. 
 * based on https://github.com/akellyirl/AutoCorr_Freq_detect.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
*/
class FrequncyAutoCorrelationStream : public AudioStream {
  public:
    FrequncyAutoCorrelationStream() = default;

    FrequncyAutoCorrelationStream(Print& out) {
      p_out = &out;
    };

    FrequncyAutoCorrelationStream(Stream& in) {
      p_out = &in;
      p_in = &in;
    };

    bool begin(AudioInfo info){
      setAudioInfo(info);
      return AudioStream::begin();
    }

    int available()override{
      if (p_in) return p_in->available();
      return 0;
    }

    int availableForWrite()override{
      if (p_out) return p_out->availableForWrite();
      return DEFAULT_BUFFER_SIZE;
    }

    size_t readBytes(uint8_t *data, size_t len) override {
      size_t result = p_in->readBytes(data, len);
      switch(info.bits_per_sample){
        case 16:
          detect<int16_t>((int16_t*)data, len/sizeof(int16_t));
          break;
        case 24:
          detect<int24_t>((int24_t*)data, len/sizeof(int24_t));
          break;
        case 32:
          detect<int32_t>((int32_t*)data, len/sizeof(int32_t));
          break;
      }
      return result;
    }

    virtual size_t write(const uint8_t *data, size_t len) override { 
      switch(info.bits_per_sample){
        case 16:
          detect<int16_t>((int16_t*)data, len/sizeof(int16_t));
          break;
        case 24:
          detect<int24_t>((int24_t*)data, len/sizeof(int24_t));
          break;
        case 32:
          detect<int32_t>((int32_t*)data, len/sizeof(int32_t));
          break;
      }

      size_t result = len;
      if (p_out!=nullptr) result = p_out->write(data, len);
      return result;
    }

    /// provides the determined frequncy
    float frequency(int channel) {
      if(channel>=info.channels) {
        LOGE("Invalid channel: %d", channel);
        return 0;
      }
      return freq[channel];
    }

  protected:
    Vector<float> freq;
    Print *p_out=nullptr;
    Stream *p_in = nullptr;
    float sum=0, sum_old=0;

    template <class T>
    void detect(T* samples, size_t len) {
      freq.resize(info.channels);
      for (int ch=0; ch<info.channels; ch++){
        freq[ch] = detectChannel(ch, samples, len);
      }
    }

    template <class T>
    float detectChannel(int channel, T* samples, size_t len) {
      int thresh = 0;
      uint8_t pd_state = 0;
      int period = 0;
      T max_value = NumberConverter::maxValue(info.bits_per_sample);      
      // Autocorrelation
      for(int i = channel; i < len; i+=info.channels) {
        sum_old = sum;
        sum = 0;
        for(int k = channel; k < len-i; k+=info.channels) {
          sum += (samples[k])*(samples[k+i]); // / max_value;
        }
        
        // Peak Detect State Machine
        if (pd_state == 2 && (sum-sum_old) <=0) {
          period = i;
          pd_state = 3;
        }
        if (pd_state == 1 && (sum > thresh) && (sum-sum_old) > 0) {
          pd_state = 2;
        }
        if (pd_state == 0) {
          thresh = sum * 0.5;
          pd_state = 1;
        }
      }
      // Frequency identified in Hz
      return static_cast<float>(info.sample_rate) / period;
    }
};

/**
 * @brief Determine Frequency using upward 0 crossings. 
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @ingroup io
*/

class FrequncyZeroCrossingStream : public AudioStream {
  public:
    FrequncyZeroCrossingStream() = default;

    FrequncyZeroCrossingStream(Print& out) {
      p_out = &out;
    };

    FrequncyZeroCrossingStream(Stream& in) {
      p_out = &in;
      p_in = &in;
    };

    bool begin(AudioInfo info){
      setAudioInfo(info);
      return AudioStream::begin();
    }

    int available()override{
      if (p_in) return p_in->available();
      return 0;
    }

    int availableForWrite()override{
      if (p_out) return p_out->availableForWrite();
      return DEFAULT_BUFFER_SIZE;
    }

    size_t readBytes(uint8_t *data, size_t len) override {
      size_t result = p_in->readBytes(data, len);
      switch(info.bits_per_sample){
        case 16:
          detect<int16_t>((int16_t*)data, len/sizeof(int16_t));
          break;
        case 24:
          detect<int24_t>((int24_t*)data, len/sizeof(int24_t));
          break;
        case 32:
          detect<int32_t>((int32_t*)data, len/sizeof(int32_t));
          break;
      }
      return result;
    }

    virtual size_t write(const uint8_t *data, size_t len) override { 
      switch(info.bits_per_sample){
        case 16:
          detect<int16_t>((int16_t*)data, len/sizeof(int16_t));
          break;
        case 24:
          detect<int24_t>((int24_t*)data, len/sizeof(int24_t));
          break;
        case 32:
          detect<int32_t>((int32_t*)data, len/sizeof(int32_t));
          break;
      }

      size_t result = len;
      if (p_out!=nullptr) result = p_out->write(data, len);
      return result;
    }

    /// provides the determined frequncy
    float frequency(int channel) {
      if(channel>=info.channels) {
        LOGE("Invalid channel: %d", channel);
        return 0;
      }
      return freq[channel];
    }

    void setFrequencyCallback(void (*callback)(int channel, float freq)){
      notify = callback;
    }

  protected:
    Vector<float> freq;
    Print *p_out=nullptr;
    Stream *p_in = nullptr;
    int count = 0;
    bool active = false;
    void (*notify)(int channel, float freq);

    template <class T>
    void detect(T* samples, size_t len) {
      freq.resize(info.channels);
      for (int ch=0; ch<info.channels; ch++){
        detectChannel(ch, samples, len);
      }
    }

    template <class T>
    void detectChannel(int channel, T* samples, size_t len) {
      for(int i = channel; i < (len-info.channels); i+=info.channels) {
        // start counter at first crossing
        if (active) count++;
        // update frequncy at each upward zero crossing
        if (samples[i]<=0 && samples[i+info.channels]>0) {
          freq[channel] = (1.0f * info.sample_rate) / count;
          if (notify) notifyAudioChange(channel, freq[channel]);
          count = 0;
          active = true;
        }
      }      
    }
};


}
