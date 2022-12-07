#pragma once
#include "AudioTools/AudioStreams.h"

namespace audio_tools {


/**
 * @brief Converter for reducing or increasing the number of Channels
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template<typename T>
class ChannelFormatConverterStreamT : public AudioStreamX {
  public:
        ChannelFormatConverterStreamT(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }
        ChannelFormatConverterStreamT(Print &print){
          p_print = &print;
        }
        ChannelFormatConverterStreamT(ChannelFormatConverterStreamT const&) = delete;
        ChannelFormatConverterStreamT& operator=(ChannelFormatConverterStreamT const&) = delete;

        bool begin(int fromChannels, int toChannels){
          from_channels = fromChannels;
          to_channels = toChannels;
          factor = static_cast<float>(toChannels) / static_cast<float>(fromChannels);

          converter.setSourceChannels(from_channels);
          converter.setTargetChannels(to_channels);

          return true;
        }

        virtual size_t write(const uint8_t *data, size_t size) override { 
           if (from_channels==to_channels){
              return p_print->write(data, size);
           }
           size_t resultBytes = convert(data, size);
           assert(resultBytes = factor*size);
           p_print->write((uint8_t*)buffer.data(), resultBytes);
           return size;
        }

        size_t readBytes(uint8_t *data, size_t size) override {
           if (p_stream==nullptr) return 0;
           if (from_channels==to_channels){
              return p_stream->readBytes(data, size);
           }
           size_t in_bytes = 1.0f / factor * size;
           bufferTmp.resize(in_bytes);
           p_stream->readBytes(bufferTmp.data(), in_bytes);
           size_t resultBytes = convert(bufferTmp.data(), in_bytes);
           assert(size==resultBytes);
           memcpy(data, (uint8_t*)buffer.data(),  size);
           return size;
        }

        void setAudioInfo(AudioBaseInfo cfg) override {
          AudioStreamX::setAudioInfo(cfg);
          to_channels = cfg.channels;
        }

        virtual int available() override {
          return p_stream!=nullptr ? factor * p_stream->available() : 0;
        }

        virtual int availableForWrite() override { 
          return 1.0f / factor * p_print->availableForWrite();
        }

  protected:
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    int from_channels = 2;
    int to_channels = 2;
    float factor = 1;
    Vector<T> buffer;
    Vector<uint8_t> bufferTmp;
    ChannelConverter<T> converter;

    size_t convert(const uint8_t *in_data, size_t size){
        size_t result;
        size_t result_samples = size/sizeof(T)*factor;
        buffer.resize(result_samples);
        result = converter.convert((uint8_t*)buffer.data(),(uint8_t*) in_data, size);
        if (result!=result_samples*sizeof(T)){
          LOGE("size %d -> result: %d - expeced: %d", (int) size, (int) result, static_cast<int>(result_samples*sizeof(T)));
        }
        return result;
    }

};

/**
 * @brief Channel converter which does not use a template
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class ChannelFormatConverterStream : public AudioStreamX {
  public:
        ChannelFormatConverterStream() = default;

        ChannelFormatConverterStream(Stream &stream){
          setStream(stream);
        }
        ChannelFormatConverterStream(Print &print){
          setStream(print);
        }
        ChannelFormatConverterStream(ChannelFormatConverterStream const&) = delete;
        ChannelFormatConverterStream& operator=(ChannelFormatConverterStream const&) = delete;

        void setStream(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }
        void setStream(Print &print) {
          p_print = &print;
        }

        void setAudioInfo(AudioBaseInfo cfg) override {
          AudioStreamX::setAudioInfo(cfg);
            switch(bits_per_sample){
              case 8:
                 static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->setAudioInfo(cfg);
                 break;
              case 16:
                 static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->setAudioInfo(cfg);
                 break;
              case 24:
                 static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->setAudioInfo(cfg);
                 break;
              case 32:
                 static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->setAudioInfo(cfg);
                 break;
            }
        }

        bool begin(int fromChannels, int toChannels, int bits_per_sample=16){
          this->bits_per_sample = bits_per_sample;
          return setupConverter(fromChannels, toChannels);
        }

        virtual size_t write(const uint8_t *data, size_t size) override { 
            switch(bits_per_sample){
              case 8:
                return static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->write(data,size);
              case 16:
                return static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->write(data,size);
              case 24:
                return static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->write(data,size);
              case 32:
                return static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->write(data,size);
              default:
                return 0;
            }
        }

        size_t readBytes(uint8_t *data, size_t size) override {
            switch(bits_per_sample){
              case 8:
                return static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->write(data,size);
              case 16:
                return static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->write(data,size);
              case 24:
                return static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->write(data,size);
              case 32:
                return static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->write(data,size);
              default:
                return 0;
            }
        }

        virtual int available() override {
            switch(bits_per_sample){
              case 8:
                return static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->available();
              case 16:
                return static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->available();
              case 24:
                return static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->available();
              case 32:
                return static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->available();
              default:
                return 0;
            }
        }

        virtual int availableForWrite() override { 
            switch(bits_per_sample){
              case 8:
                return static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->availableForWrite();
              case 16:
                return static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->availableForWrite();
              case 24:
                return static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->availableForWrite();
              case 32:
                return static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->availableForWrite();
              default:
                return 0;
            }
        }

  protected:
      Stream *p_stream=nullptr;
      Print *p_print=nullptr;
      void *converter;
      int bits_per_sample=0;

      bool setupConverter(int fromChannels, int toChannels){
        bool result;
        if (p_stream!=nullptr){
          switch(bits_per_sample){
            case 8:
              converter = new ChannelFormatConverterStreamT<int8_t>(*p_stream);
              static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 16:
              converter = new ChannelFormatConverterStreamT<int16_t>(*p_stream);
              static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 24:
              converter = new ChannelFormatConverterStreamT<int24_t>(*p_stream);
              static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 32:
              converter = new ChannelFormatConverterStreamT<int32_t>(*p_stream);
              static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            default:
              result = false;
          }
        } else {
          switch(bits_per_sample){
            case 8:
              converter = new ChannelFormatConverterStreamT<int8_t>(*p_print);
              static_cast<ChannelFormatConverterStreamT<int8_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 16:
              converter = new ChannelFormatConverterStreamT<int16_t>(*p_print);
              static_cast<ChannelFormatConverterStreamT<int16_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 24:
              converter = new ChannelFormatConverterStreamT<int24_t>(*p_print);
              static_cast<ChannelFormatConverterStreamT<int24_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            case 32:
              converter = new ChannelFormatConverterStreamT<int32_t>(*p_print);
              static_cast<ChannelFormatConverterStreamT<int32_t>*>(converter)->begin(fromChannels,toChannels);
              break;
            default:
              result = false;
          }
        }
        return result;
        
      }

};


/**
 * @brief Converter which converts from source bits_per_sample to target bits_per_sample
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T specifies the current data type for the result of the read or write. 
 * @tparam TArg is the data type of the Stream or Print Object that is passed in the Constructor
 */

template<typename TFrom, typename TTo >
class NumberFormatConverterStreamT : public AudioStreamX {
  public:
        NumberFormatConverterStreamT() = default;

        NumberFormatConverterStreamT(Stream &stream){
          setStream(stream);
        }
        NumberFormatConverterStreamT(Print &print){
          setStream(print);
        }

        void setStream(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }
        void setStream(Print &print){
          p_print = &print;
        }

        bool begin() override {
          return true;
        }

        virtual size_t write(const uint8_t *data, size_t size) override { 
           size_t samples = size / sizeof(TFrom);
           size_t result_size = 0;
           TFrom *data_source = (TFrom *)data;
           
           for (size_t j=0;j<samples;j++){
             TTo value = static_cast<float>(data_source[j]) * NumberConverter::maxValue(sizeof(TTo)*8) / NumberConverter::maxValue(sizeof(TFrom)*8);
             result_size += p_print->write((uint8_t*)&value, sizeof(TTo));
           }
           //assert(result_size / sizeof(TTo) * sizeof(TFrom)==size);
           return size;;
        }

        size_t readBytes(uint8_t *data, size_t size) override {
           if (p_stream==nullptr) return 0;
           size_t samples = size / sizeof(TTo);
           TTo *data_target = (TTo *)data;
           TFrom source;
           for (size_t j=0;j<samples;j++){
             source = 0;
             p_stream->readBytes((uint8_t*)&source, sizeof(TFrom));
             data_target[j]= static_cast<float>(source) * NumberConverter::maxValue(sizeof(TTo)*8) / NumberConverter::maxValue(sizeof(TFrom)*8);
           }
           return size;
        }

        virtual int available() override {
          return p_stream!=nullptr ? p_stream->available() : 0;
        }

        virtual int availableForWrite() override { 
          return p_print->availableForWrite();
        }

  protected:
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
};

/**
 * @brief Converter which converts between bits_per_sample and 16 bits
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class NumberFormatConverterStream :  public AudioStreamX {
  public:
        NumberFormatConverterStream() = default;

        NumberFormatConverterStream(Stream &stream){
          setStream(stream);
        }
        NumberFormatConverterStream(Print &print) {
          setStream(print);
        }

        void setStream(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }
        void setStream(Print &print) {
          p_print = &print;
        }

        bool begin(int from_bit_per_samples, int to_bit_per_samples){
          bool result = true;
          this->from_bit_per_samples = from_bit_per_samples;
          this->to_bit_per_samples = to_bit_per_samples;
    
          if (from_bit_per_samples==to_bit_per_samples){
            LOGI("no bit combination: %d -> %d",from_bit_per_samples, to_bit_per_samples);
          } else if (from_bit_per_samples==8 &&to_bit_per_samples==16){
            converter = new NumberFormatConverterStreamT<int8_t,int16_t>();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
            converter = new NumberFormatConverterStreamT<int16_t,int8_t>();
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
            converter = new NumberFormatConverterStreamT<int32_t,int16_t>();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
            converter = new NumberFormatConverterStreamT<int16_t,int32_t>();
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
            converter = new NumberFormatConverterStreamT<int32_t,int16_t>();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
            converter = new NumberFormatConverterStreamT<int16_t,int32_t>();
          } else {
            result = false;
            LOGE("bit combination not supported %d -> %d",from_bit_per_samples, to_bit_per_samples);
          }

          setupStream();

          return result;
        }

        virtual size_t write(const uint8_t *data, size_t size) override { 
          if (from_bit_per_samples==to_bit_per_samples){
            return p_print->write(data, size);
          }

          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->write(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
             return static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->write(data,size);
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->write(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->write(data,size);
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->write(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->write(data,size);
          } else {
            return 0;
          }
        }

        size_t readBytes(uint8_t *data, size_t size) override {
          if (from_bit_per_samples==to_bit_per_samples){
            return p_stream->readBytes(data, size);
          }
          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->readBytes(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
             return static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->readBytes(data,size);
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->readBytes(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->readBytes(data,size);
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->readBytes(data,size);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->readBytes(data,size);
          } else {
            return 0;
          } 
        }

        virtual int available() override {
          if (from_bit_per_samples==to_bit_per_samples){
            return p_stream->available();
          }
          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->available();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
             return static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->available();
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->available();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->available();
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->available();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->available();
          } else {
            return 0;
          } 
        }

        virtual int availableForWrite() override { 
          if (from_bit_per_samples==to_bit_per_samples){
            return p_print->availableForWrite();
          }
          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->availableForWrite();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
             return static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->availableForWrite();
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->availableForWrite();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->availableForWrite();
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
             return static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->availableForWrite();
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
             return static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->availableForWrite();
          } else {
            return 0;
          } 
        }

  protected:
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    void *converter=nullptr;
    int from_bit_per_samples=0;
    int to_bit_per_samples=0;

    void setupStream() {
        if (p_stream!=nullptr){
          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->setStream(*p_stream);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
              static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->setStream(*p_stream);
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->setStream(*p_stream);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
              static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->setStream(*p_stream);
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->setStream(*p_stream);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
              static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->setStream(*p_stream);
          } 
        } else {
          if (from_bit_per_samples==8 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int8_t,int16_t>*>(converter)->setStream(*p_print);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==8){
              static_cast<NumberFormatConverterStreamT<int16_t,int8_t>*>(converter)->setStream(*p_print);
          } else if (from_bit_per_samples==24 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->setStream(*p_print);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==24){
              static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->setStream(*p_print);
          } else if (from_bit_per_samples==32 &&to_bit_per_samples==16){
              static_cast<NumberFormatConverterStreamT<int32_t,int16_t>*>(converter)->setStream(*p_print);
          } else if (from_bit_per_samples==16 &&to_bit_per_samples==32){
              static_cast<NumberFormatConverterStreamT<int16_t,int32_t>*>(converter)->setStream(*p_print);
          } 
        }
    }
};


/**
 * @brief Converter which converts bits_per_sample and channels
 * @ingroup transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class FormatConverterStream : public AudioStreamX {
  public:
        FormatConverterStream() = default;

        FormatConverterStream(Stream &stream){
          setStream(stream);
        }
        FormatConverterStream(Print &print){
          setStream(print);
        }

        FormatConverterStream(AudioStream &stream){
          setSourceAudioInfo(stream.audioInfo());  
          setStream(stream);
        }
        FormatConverterStream(AudioPrint &print){
          setSourceAudioInfo(print.audioInfo());  
          setStream(print);
        }

        void setStream(Stream &stream){
          p_stream = &stream;
          p_print = &stream;
        }

        void setStream(Print &print){
          p_print = &print;
        }

        /// Defines the audio info of the stream which has been defined in the constructor
        void setSourceAudioInfo(AudioBaseInfo from){
          from_cfg = from;
        }

        bool begin(AudioBaseInfo from, AudioBaseInfo to){
            setSourceAudioInfo(from);
            return begin(to);
        }

        bool begin(AudioBaseInfo to){
          setAudioInfo(to);
          to_cfg = to;
          if (p_stream!=nullptr){
            numberFormatConverter.setStream(*p_stream);
            channelFormatConverter.setStream(numberFormatConverter);
          } else {
            numberFormatConverter.setStream(*p_print);
            channelFormatConverter.setStream(numberFormatConverter);
          }
          bool result = numberFormatConverter.begin(from_cfg.bits_per_sample, to_cfg.bits_per_sample);
          if (result){
            result =  channelFormatConverter.begin(to_cfg.bits_per_sample, from_cfg.channels, to_cfg.channels);
          }
          return result;
        }

        virtual size_t write(const uint8_t *data, size_t size) override { 
          return channelFormatConverter.write(data, size);
        }

        size_t readBytes(uint8_t *data, size_t size) override {
          return channelFormatConverter.readBytes(data, size);
        }

        virtual int available() override {
          return channelFormatConverter.available();
        }

        virtual int availableForWrite() override { 
          return channelFormatConverter.availableForWrite();
        }

  protected:
    AudioBaseInfo from_cfg;
    AudioBaseInfo to_cfg;
    Stream *p_stream=nullptr;
    Print *p_print=nullptr;
    NumberFormatConverterStream numberFormatConverter;
    ChannelFormatConverterStream channelFormatConverter;
};


} // namespace