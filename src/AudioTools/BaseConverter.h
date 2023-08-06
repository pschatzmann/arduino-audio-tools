#pragma once
#include "AudioTypes.h"
#include "AudioBasic/Collections.h"
#include "AudioFilter/Filter.h"

/**
 * @defgroup convert Converters
 * @ingroup tools
 * @brief Convert Audio
 * You can add a converter as argument to the StreamCopy::copy() or better use is with 
 * a ConverterStream.
 */

namespace audio_tools {


/**
 * @brief Abstract Base class for Converters
 * A converter is processing the data in the indicated array
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
class BaseConverter {
    public:
        BaseConverter() = default;
        BaseConverter(BaseConverter const&) = delete;
        virtual ~BaseConverter() = default;

        BaseConverter& operator=(BaseConverter const&) = delete;

        virtual size_t convert(uint8_t *src, size_t size) = 0;
};


/**
 * @brief Dummy converter which does nothing
 * @ingroup convert
 * @tparam T 
 */
class NOPConverter : public  BaseConverter {
    public:
        virtual size_t convert(uint8_t(*src), size_t size) {return size;};
};

/**
 * @brief Multiplies the values with the indicated factor adds the offset and clips at maxValue. To mute use a factor of 0.0!
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterScaler : public  BaseConverter {
    public:
        ConverterScaler(float factor, T offset, T maxValue, int channels=2){
            this->factor_value = factor;
            this->maxValue = maxValue;
            this->offset_value = offset;
            this->channels = channels;
        }

        size_t convert(uint8_t*src, size_t byte_count) {
            T *sample = (T*)src;
            int size = byte_count / channels / sizeof(T);
            for (size_t j=0;j<size;j++){
                for (int i=0;i<channels;i++){
                    *sample = (*sample + offset_value) * factor_value;
                    if (*sample>maxValue){
                        *sample = maxValue;
                    } else if (*sample<-maxValue){
                        *sample = -maxValue;
                    }
                    sample++;
                }
            }
            return byte_count;
        }

        /// Defines the factor (volume)
        void setFactor(T factor){
            this->factor_value = factor;
        }

        /// Defines the offset
        void setOffset(T offset) {
            this->offset_value = offset;
        }

        /// Determines the actual factor (volume)
        float factor() {
            return factor_value;
        }

        /// Determines the offset value
        T offset() {
            return offset_value;
        }

    protected:
        int channels;
        float factor_value;
        T maxValue;
        T offset_value;
};

/**
 * @brief Makes sure that the avg of the signal is set to 0
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class ConverterAutoCenterT : public  BaseConverter {
    public:
        ConverterAutoCenterT(int channels=2){
            this->channels = channels;
        }

        size_t convert(uint8_t(*src), size_t byte_count) {
            int size = byte_count / channels / sizeof(T);
            T *sample = (T*) src;
            setup((T*)src, size);
            // convert data
            if (is_setup){
                for (size_t j=0; j<size; j++){
                    for (int i=0;i<channels;i++){
                        *sample = *sample - offset;
                        sample++;
                    }
                }
            }
            return byte_count;
        }

    protected:
        T offset = 0;
        float left = 0.0;
        float right = 0.0;
        bool is_setup = false;
        int channels;

        void setup(T *src, size_t size){
            if (size==0) return;
            if (!is_setup) {
                if (channels==1){
                    T *sample = (T*) src;
                    for (size_t j=0;j<size;j++){
                        left += *sample++;
                    }
                    offset = left / size;
                    is_setup = true;
                    LOGD("offset: %d",(int)offset);
                } else if (channels==2){
                    T *sample = (T*) src;
                    for (size_t j=0;j<size;j++){
                        left += *sample++;
                        right += *sample++;
                    }
                    left = left / size;
                    right = right / size;

                    if (left>0){
                        offset = left;
                        is_setup = true;
                    } else if (right>0){
                        offset = right;
                        is_setup = true;
                    }
                    LOGD("offset: %d",(int)offset);
                }
            }
        }
};

/**
 * @brief Makes sure that the avg of the signal is set to 0
 * @ingroup convert
 */
class ConverterAutoCenter : public  BaseConverter {
  public:
    ConverterAutoCenter() = default;

    ConverterAutoCenter(AudioInfo info){
        begin(info.channels, info.bits_per_sample);
    }

    ConverterAutoCenter(int channels, int bitsPerSample){
        begin(channels, bitsPerSample);
    }
    ~ConverterAutoCenter(){
        if (p_converter!=nullptr) delete p_converter;
    }

    void begin(int channels, int bitsPerSample){
        this->channels = channels;
        this->bits_per_sample = bitsPerSample;
        if (p_converter!=nullptr) delete p_converter;
        switch(bits_per_sample){
            case 16:{
                p_converter = new ConverterAutoCenterT<int16_t>(channels);
                break;
            }
            case 24:{
                p_converter = new ConverterAutoCenterT<int24_t>(channels);
                break;
            }
            case 32:{
                p_converter = new ConverterAutoCenterT<int32_t>(channels);
                break;
            }
        }
    }

    size_t convert(uint8_t *src, size_t size) override {
        if(p_converter==nullptr) return 0;
        return p_converter->convert(src, size);
    }


  protected:
    int channels;
    int bits_per_sample;
    BaseConverter *p_converter = nullptr;
};



/**
 * @brief Switches the left and right channel
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterSwitchLeftAndRight : public  BaseConverter {
    public:
        ConverterSwitchLeftAndRight(int channels=2){
            this->channels = channels;
        }

        size_t convert(uint8_t*src, size_t byte_count) {
            if (channels==2){
                int size = byte_count / channels / sizeof(T);
                T *sample = (T*) src;
                for (size_t j=0;j<size;j++){
                    T temp = *sample;
                    *sample = *(sample+1)
                    *(sample+1) = temp;
                    sample+=2;
                }
            }
            return byte_count;
        }
    protected:
        int channels=2;
};


enum FillLeftAndRightStatus {Auto, LeftIsEmpty, RightIsEmpty};

/**
 * @brief Make sure that both channels contain any data
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */


template<typename T>
class ConverterFillLeftAndRight : public  BaseConverter {
    public:
        ConverterFillLeftAndRight(FillLeftAndRightStatus config=Auto, int channels=2){
            this->channels = channels;
            switch(config){
                case LeftIsEmpty:
                    left_empty = true;
                    right_empty = false;
                    is_setup = true;
                    break;
                case RightIsEmpty:
                    left_empty = false;
                    right_empty = true;
                    is_setup = true;
                    break;
                case Auto:
                    is_setup = false;
                    break;
            }
        }

        size_t convert(uint8_t*src, size_t byte_count) {
            if (channels==2){
                int size = byte_count / channels / sizeof(T);
                setup((T*)src, size);
                if (left_empty && !right_empty){
                    T *sample = (T*)src;
                    for (size_t j=0;j<size;j++){
                        *sample = *(sample+1);
                        sample+=2;
                    }
                } else if (!left_empty && right_empty) {
                    T *sample = (T*)src;
                    for (size_t j=0;j<size;j++){
                        *(sample+1) = *sample;
                        sample+=2;
                    }
                }
            }
            return byte_count;
        }

    private:
        bool is_setup = false;
        bool left_empty = true;
        bool right_empty = true; 
        int channels;

        void setup(T *src, size_t size) {
            if (!is_setup) {
                for (int j=0;j<size;j++) {
                    if (*src!=0) {
                        left_empty = false;
                        break;
                    }
                    src+=2;
                }
                for (int j=0;j<size-1;j++) {
                    if (*(src)!=0) {
                        right_empty = false;
                        break;
                    }
                    src+=2;
                }
                // stop setup as soon as we found some data
                if (!right_empty || !left_empty) {
                    is_setup = true;
                }
            }
        }

};

/**
 * @brief special case for internal DAC output, the incomming PCM buffer needs 
 *  to be converted from signed 16bit to unsigned
 * @ingroup convert
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterToInternalDACFormat : public  BaseConverter {
    public:
        ConverterToInternalDACFormat(int channels=2){
            this->channels = channels;
        }

        size_t convert(uint8_t*src, size_t byte_count) {
            int size = byte_count / channels / sizeof(T);
            T *sample = (T*) src;
            for (int i=0; i<size; i++) {
                for (int j=0;j<channels;j++){
                    *sample = *sample + 0x8000;
                    sample++;
                }
            }
            return byte_count;
        }
    protected:
        int channels;
};

/**
 * @brief We combine a datastream which consists of multiple channels into less channels. E.g. 2 to 1
 * The last target channel will contain the combined values of the exceeding source channels.
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class ChannelReducerT : public BaseConverter {
    public:
        ChannelReducerT() = default;

        ChannelReducerT(int channelCountOfTarget, int channelCountOfSource){
            from_channels = channelCountOfSource;
            to_channels = channelCountOfTarget;
        }

        void setSourceChannels(int channelCountOfSource) {
            from_channels = channelCountOfSource;
        }

        void setTargetChannels(int channelCountOfTarget) {
            to_channels = channelCountOfTarget;
        }

        size_t convert(uint8_t*src, size_t size) {
            return convert(src,src,size);
        }

        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            LOGD("convert %d -> %d", from_channels, to_channels);
            assert(to_channels<=from_channels);
            int frame_count = size/(sizeof(T)*from_channels);
            size_t result_size=0;
            T* result = (T*)target;
            T* source = (T*)src;
            int reduceDiv = from_channels - to_channels + 1;

            for(int i=0; i < frame_count; i++){
                // copy first to_channels-1 
                for (int j=0;j<to_channels-1;j++){
                    *result++ = *source++;
                    result_size += sizeof(T);
                }
                // commbined last channels
                T total = (int16_t)0;
                for (int j=to_channels-1;j<from_channels;j++){
                    total += *source++ / reduceDiv;
                }                
                *result++ = total;
                result_size += sizeof(T);
            }
            return result_size;
        }

    protected:
        int from_channels;
        int to_channels;
};

/**
 * @brief We combine a datastream which consists of multiple channels into less channels. E.g. 2 to 1
 * The last target channel will contain the combined values of the exceeding source channels.
 * @ingroup convert
 */
class ChannelReducer : public BaseConverter {
    public:

        ChannelReducer(int channelCountOfTarget, int channelCountOfSource, int bitsPerSample){
            from_channels = channelCountOfSource;
            to_channels = channelCountOfTarget;
            bits = bitsPerSample;
        }

        size_t convert(uint8_t*src, size_t size) {
            return convert(src,src,size);
        }

        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            switch(bits){
                case 16:{
                    ChannelReducerT<int16_t> cr16(to_channels, from_channels);
                    return cr16.convert(target, src, size);
                }
                case 24:{
                    ChannelReducerT<int24_t> cr24(to_channels, from_channels);
                    return cr24.convert(target, src, size);
                }
                case 32:{
                    ChannelReducerT<int32_t> cr32(to_channels, from_channels);
                    return cr32.convert(target, src, size);
                }
            }
            return 0;
        }

    protected:
        int from_channels;
        int to_channels;
        int bits;
};


/**
 * @brief Provides reduced sampling rates
 * @ingroup convert
*/
template<typename T>
class DecimateT : public BaseConverter {
    public:
        DecimateT(int factor, int channels){
            setChannels(channels);
            setFactor(factor);

        }
        /// Defines the number of channels
        void setChannels(int channels){
            this->channels = channels;
        }

        /// Sets the factor: e.g. with 4 we keep every forth sample
        void setFactor(int factor){
            this->factor = factor;
        }

        size_t convert(uint8_t*src, size_t size) {
            return convert(src, src, size);
        }

        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            int frame_count = size/(sizeof(T)*channels);
            T *p_target = (T*) target;
            T *p_source = (T*) src;
            size_t result_size = 0;

            for(int i=0; i < frame_count; i++){
                if (++count == factor){
                    count = 0;
                    // only keep even samples
                    for (int ch=0; ch<channels; ch++){
                        *p_target++ = p_source[i+ch]; 
                        result_size += sizeof(T);
                    }
                }
            }
            //LOGI("%d: %d -> %d ",factor, (int)size, (int)result_size);
            return result_size;
        }

        operator bool() {return factor>1;};

    protected:
        int channels=2;
        int factor=1;
        uint16_t count=0;
};

/**
 * @brief Provides reduced the sampling rates
 * @ingroup convert
*/

class Decimate : public BaseConverter {
    public:
        Decimate() = default;
        Decimate(int factor, int channels, int bits_per_sample){
            setFactor(factor);
            setChannels(channels);
            setBits(bits_per_sample);
        }
        /// Defines the number of channels
        void setChannels(int channels){
            this->channels = channels;
        }
        void setBits(int bits){
            this->bits = bits;
        }
        /// Sets the factor: e.g. with 4 we keep every forth sample
        void setFactor(int factor){
            this->factor = factor;
        }

        size_t convert(uint8_t*src, size_t size) {
            return convert(src, src, size);
        }
        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            switch(bits){
                case 16:{
                    DecimateT<int16_t> dec16(factor, channels);
                    return dec16.convert(target, src, size);
                }
                case 24:{
                    DecimateT<int24_t> dec24(factor, channels);
                    return dec24.convert(target, src, size);
                }
                case 32:{
                    DecimateT<int32_t> dec32(factor, channels);
                    return dec32.convert(target, src, size);
                }
            }
            return 0;
        }

        operator bool() {return factor>1;};

    protected:
        int channels=2;
        int bits=16;
        int factor=1;

};


/**
 * @brief Increases the channel count
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class ChannelEnhancer  {
    public:
        ChannelEnhancer() = default;

        ChannelEnhancer(int channelCountOfTarget, int channelCountOfSource){
            from_channels = channelCountOfSource;
            to_channels = channelCountOfTarget;
        }

        void setSourceChannels(int channelCountOfSource) {
            from_channels = channelCountOfSource;
        }

        void setTargetChannels(int channelCountOfTarget) {
            to_channels = channelCountOfTarget;
        }

        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            int frame_count = size/(sizeof(T)*from_channels);
            size_t result_size=0;
            T* result = (T*)target;
            T* source = (T*)src;
            T value = (int16_t)0;
            for(int i=0; i < frame_count; i++){
                // copy available channels
                for (int j=0;j<from_channels;j++){
                    value = *source++;
                    *result++ = value;
                    result_size += sizeof(T);
                }
                // repeat last value
                for (int j=from_channels;j<to_channels;j++){
                    *result++ = value;
                    result_size += sizeof(T);
                }                
            }
            return result_size;
        }

        /// Determine the size of the conversion result
        size_t resultSize(size_t inSize){
            return inSize * to_channels / from_channels;
        }

    protected:
        int from_channels;
        int to_channels;
};

/**
 * @brief Increasing or decreasing the number of channels
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class ChannelConverter {
    public:
        ChannelConverter() = default;

        ChannelConverter(int channelCountOfTarget, int channelCountOfSource){
            from_channels = channelCountOfSource;
            to_channels = channelCountOfTarget;
        }

        void setSourceChannels(int channelCountOfSource) {
            from_channels = channelCountOfSource;
        }

        void setTargetChannels(int channelCountOfTarget) {
            to_channels = channelCountOfTarget;
        }

        size_t convert(uint8_t*target, uint8_t*src, size_t size) {
            if (from_channels==to_channels){
                memcpy(target,src,size);
                return size;
            }
            // setup channels
            if (from_channels>to_channels){
                reducer.setSourceChannels(from_channels);
                reducer.setTargetChannels(to_channels);
            } else {
                enhancer.setSourceChannels(from_channels);
                enhancer.setTargetChannels(to_channels);
            }

            // execute conversion
            if (from_channels>to_channels){
                return reducer.convert(target, src, size);
            } else {
                return enhancer.convert(target, src, size);
            }
        }

    protected:
        ChannelEnhancer<T> enhancer;
        ChannelReducerT<T> reducer;
        int from_channels;
        int to_channels;

};

/**
 * @brief Combines multiple converters
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class MultiConverter : public BaseConverter {
    public:
        MultiConverter(){
        }

        MultiConverter(BaseConverter &c1){
            add(c1);
        }

        MultiConverter(BaseConverter &c1, BaseConverter &c2){
            add(c1);
            add(c2);
        }

        MultiConverter(BaseConverter &c1, BaseConverter &c2, BaseConverter &c3){
            add(c1);
            add(c2);
            add(c3);
        }

        // adds a converter
        void add(BaseConverter &converter){
            converters.push_back(&converter);
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        size_t convert(uint8_t*src, size_t size) {
            for(int i=0; i < converters.size(); i++){
                converters[i]->convert(src, size);
            }
            return size;
        }

    private:
        Vector<BaseConverter*> converters;

};

// /**
//  * @brief Converts e.g. 24bit data to the indicated smaller or bigger data type
//  * @ingroup convert
//  * @author Phil Schatzmann
//  * @copyright GPLv3
//  * 
//  * @tparam T 
//  */
// template<typename FromType, typename ToType>
// class FormatConverter {
//     public:
//         FormatConverter(ToType (*converter)(FromType v)){
//             this->convert_ptr = converter;
//         }

//         FormatConverter( float factor, float clip){
//             this->factor = factor;
//             this->clip = clip;
//         }

//         // The data is provided as int24_t tgt[][2] but  returned as int24_t
//         size_t convert(uint8_t *src, uint8_t *target, size_t byte_count_src) {
//             return convert((FromType *)src, (ToType*)target, byte_count_src );
//         }

//         // The data is provided as int24_t tgt[][2] but  returned as int24_t
//         size_t convert(FromType *src, ToType *target, size_t byte_count_src) {
//             int size = byte_count_src / sizeof(FromType);
//             FromType *s = src;
//             ToType *t = target;
//             if (convert_ptr!=nullptr){
//                 // standard conversion
//                 for (int i=size; i>0; i--) {
//                     *t = (*convert_ptr)(*s);
//                     t++;
//                     s++;
//                 }
//             } else {
//                 // conversion using indicated factor
//                 for (int i=size; i>0; i--) {
//                     float tmp = factor * *s;
//                     if (tmp>clip){
//                         tmp=clip;
//                     } else if (tmp<-clip){
//                         tmp = -clip;
//                     }
//                     *t = tmp;
//                     t++;
//                     s++;
//                 }
//             }
//             return size*sizeof(ToType);
//         }

//     private:
//         ToType (*convert_ptr)(FromType v) = nullptr;
//         float factor=0;
//         float clip=0;

// };



/**
 * @brief Reads n numbers from an Arduino Stream
 * 
 */
class NumberReader {
    public:
        NumberReader(Stream &in) {
            stream_ptr = &in;
        }

        NumberReader() {
        }

        bool read(int inBits, int outBits, bool outSigned, int n, int32_t *result){
            bool result_bool = false;
            int len = inBits/8 * n;
            if (stream_ptr!=nullptr && stream_ptr->available()>len){
                uint8_t buffer[len];
                stream_ptr->readBytes((uint8_t*)buffer, n * len);
                result_bool = toNumbers((void*)buffer, inBits, outBits, outSigned, n, result);
            }
            return result_bool;
        }

        /// converts a buffer to a number array
        bool toNumbers(void *bufferIn, int inBits, int outBits, bool outSigned, int n, int32_t *result){
            bool result_bool = false;
            switch(inBits){
                case 8: {
                        int8_t *buffer=(int8_t *)bufferIn;
                        for (int j=0;j<n;j++){
                            result[j] = scale(buffer[j],inBits,outBits,outSigned);
                        }
                        result_bool = true;
                    }
                    break;
                case 16: {
                        int16_t *buffer=(int16_t *)bufferIn;
                        for (int j=0;j<n;j++){
                            result[j] = scale(buffer[j],inBits,outBits,outSigned);
                        }
                        result_bool = true;
                    }
                    break;
                case 32: {
                        int32_t *buffer=(int32_t*)bufferIn;
                        for (int j=0;j<n;j++){
                            result[j] = scale(buffer[j],inBits,outBits,outSigned);
                        }
                        result_bool = true;
                    }
                    break;
            }
            return result_bool;

        }

    protected:
        Stream *stream_ptr=nullptr;

        /// scale the value
        int32_t scale(int32_t value, int inBits, int outBits, bool outSigned=true){
            int32_t result = static_cast<float>(value) / NumberConverter::maxValue(inBits) * NumberConverter::maxValue(outBits);
            if (!outSigned){
                result += (NumberConverter::maxValue(outBits) / 2);
            }
            return result;
        }

};


/**
 * @brief Converter for 1 Channel which applies the indicated Filter
 * @ingroup convert
 * @author pschatzmann
 * @tparam T
 */
template <typename T>
class Converter1Channel : public BaseConverter {
 public:
  Converter1Channel(Filter<T> &filter) { this->p_filter = &filter; }

  size_t convert(uint8_t *src, size_t size) {
    T *data = (T *)src;
    for (size_t j = 0; j < size; j++) {
      data[j] = p_filter->process(data[j]);
    }
    return size;
  }

 protected:
  Filter<T> *p_filter = nullptr;
};

/**
 * @brief Converter for n Channels which applies the indicated Filter
 * @ingroup convert
 * @author pschatzmann
 * @tparam T
 */
template <typename T, typename FT>
class ConverterNChannels : public BaseConverter {
 public:
  /// Default Constructor
  ConverterNChannels(int channels) {
    this->channels = channels;
    filters = new Filter<FT> *[channels];
    // make sure that we have 1 filter per channel
    for (int j = 0; j < channels; j++) {
      filters[j] = nullptr;
    }
  }

  /// Destrucotr
  ~ConverterNChannels() {
    for (int j = 0; j < channels; j++) {
      if (filters[j]!=nullptr){
        delete filters[j];
      }
    }
    delete[] filters;
    filters = 0;
  }

  /// defines the filter for an individual channel - the first channel is 0
  void setFilter(int channel, Filter<FT> *filter) {
    if (channel<channels){
      if (filters[channel]!=nullptr){
        delete filters[channel];
      }
      filters[channel] = filter;
    } else {
      LOGE("Invalid channel nummber %d - max channel is %d", channel, channels-1);
    }
  }

  // convert all samples for each channel separately
  size_t convert(uint8_t *src, size_t size) {
    int count = size / channels / sizeof(T);
    T *sample = (T *)src;
    for (size_t j = 0; j < count; j++) {
      for (int channel = 0; channel < channels; channel++) {
        if (filters[channel]!=nullptr){
          *sample = filters[channel]->process(*sample);
        }
        sample++;
      }
    }
    return size;
  }

  int getChannels() {
    return channels;
  }

 protected:
  Filter<FT> **filters = nullptr;
  int channels;
};

/**
 * @brief Removes any silence from the buffer that is longer then n samples with a amplitude
 * below the indicated threshhold. If you process multiple channels you need to multiply the
 * channels with the number of samples to indicate n
 * @ingroup convert
 * @tparam T 
 */

template <typename T>
class SilenceRemovalConverter : public BaseConverter  {
 public:

  SilenceRemovalConverter(int n = 8, int aplidudeLimit = 2) { 
  	set(n, aplidudeLimit);
  }

  virtual size_t convert(uint8_t *data, size_t size) override {
    if (!active) {
      // no change to the data
      return size;
    }
    size_t sample_count = size / sizeof(T);
    size_t write_count = 0;
    T *audio = (T *)data;

    // find relevant data
    T *p_buffer = (T *)data;
    for (int j = 0; j < sample_count; j++) {
      int pos = findLastAudioPos(audio, j);
      if (pos < n) {
        write_count++;
        *p_buffer++ = audio[j];
      }
    }
    
    // write audio data w/o silence
    size_t write_size = write_count * sizeof(T);
    LOGI("filtered silence from %d -> %d", (int)size, (int)write_size);

    // number of empty samples of prior buffer
    priorLastAudioPos =  findLastAudioPos(audio, sample_count - 1);

    // return new data size
    return write_size;
  }
  
 protected:
  bool active = false;
  const uint8_t *buffer = nullptr;
  int n;
  int priorLastAudioPos = 0;
  int amplidude_limit = 0;

  void set(int n = 5, int aplidudeLimit = 2) {
    LOGI("begin(n=%d, aplidudeLimit=%d", n, aplidudeLimit);
    this->n = n;
    this->amplidude_limit = aplidudeLimit;
    this->priorLastAudioPos = n+1;  // ignore first values
    this->active = n > 0;
  }


  // find last position which contains audible data
  int findLastAudioPos(T *audio, int pos) {
    for (int j = 0; j < n; j++) {
      // we are before the start of the current buffer
      if (pos - j <= 0) {
        return priorLastAudioPos;
      }
      // we are in the current buffer
      if (abs(audio[pos - j]) > amplidude_limit) {
        return j;
      }
    }
    return n + 1;
  }
};

/**
 * @brief Big value gaps (at the beginning and the end of a recording) can lead to some popping sounds.
 * We will try to set the values to 0 until the first transition thru 0 of the audio curve
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class PoppingSoundRemover : public BaseConverter {
    public:
        PoppingSoundRemover(int channels, bool fromBeginning, bool fromEnd){
            this->channels = channels;
            from_beginning = fromBeginning;
            from_end = fromEnd; 
        }
        virtual size_t convert(uint8_t *src, size_t size) {
            for (int ch=0;ch<channels;ch++){
                if (from_beginning)
                    clearUpTo1stTransition(channels, ch, (T*) src, size/sizeof(T));
                if (from_end)
                    clearAfterLastTransition(channels, ch, (T*) src, size/sizeof(T));
            }
            return size;
        }
        
    protected:
        bool from_beginning;
        bool from_end;
        int channels;

        void clearUpTo1stTransition(int channels, int channel, T* values, int sampleCount){
            T first = values[channel];
            for (int j=0;j<sampleCount;j+=channels){
                T act = values[j];
                if ((first <= 0.0 && act >= 0.0) || (first>=0.0 && act <= 0.0)){
                    // we found the last transition so we are done
                    break;
                } else {
                    values[j] = 0;
                }
            }
        }

        void clearAfterLastTransition(int channels, int channel, T* values, int sampleCount){
            int lastPos = sampleCount - channels + channel;
            T last = values[lastPos];
            for (int j=lastPos;j>=0;j-=channels){
                T act = values[j];
                if ((last <= 0.0 && act >= 0.0) || (last>=0.0 && act <= 0.0)){
                    // we found the last transition so we are done
                    break;
                } else {
                    values[j] = 0;
                }
            }
        }
};

/**
 * @brief Changes the samples at the beginning or at the end to slowly ramp up the volume
 * @ingroup convert
 * @tparam T 
 */
template<typename T>
class SmoothTransition : public BaseConverter {
    public:
        SmoothTransition(int channels, bool fromBeginning, bool fromEnd, float inc=0.01){
            this->channels = channels;
            this->inc = inc;
            from_beginning = fromBeginning;
            from_end = fromEnd; 
        }
        virtual size_t convert(uint8_t *src, size_t size) {
            for (int ch=0;ch<channels;ch++){
                if (from_beginning)
                    processStart(channels, ch, (T*) src, size/sizeof(T));
                if (from_end)
                    processEnd(channels, ch, (T*) src, size/sizeof(T));
            }
            return size;
        }
        
    protected:
        bool from_beginning;
        bool from_end;
        int channels;
        float inc=0.01;
        float factor = 0;

        void processStart(int channels, int channel, T* values, int sampleCount){
            for (int j=0;j<sampleCount;j+=channels){
                if (factor>=0.8){
                    break;
                } else {
                    values[j]=factor*values[j];
                }
                factor += inc;
            }
        }

        void processEnd(int channels, int channel, T* values, int sampleCount){
            int lastPos = sampleCount - channels + channel;
            for (int j=lastPos;j>=0;j-=channels){
                if (factor>=0.8){
                    break;
                } else {
                    values[j]=factor*values[j];
                }
            }
        }
};



}