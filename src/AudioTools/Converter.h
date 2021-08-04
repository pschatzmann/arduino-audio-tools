#pragma once
#include "AudioTypes.h"
#include "Vector.h"

namespace audio_tools {


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

static int64_t maxValue(int value_bits_per_sample){
    switch(value_bits_per_sample/8){
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

static int16_t convert16(int value, int value_bits_per_sample){
    return value * maxValue(16) / maxValue(value_bits_per_sample);

}

static int16_t convert8(int value, int value_bits_per_sample){
    return value * maxValue(8) / maxValue(value_bits_per_sample);
}



/**
 * @brief Abstract Base class for Converters
 * A converter is processing the data in the indicated array
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T 
 */
template<typename T>
class BaseConverter {
    public:
        virtual void convert(T (*src)[2], size_t size) = 0;
};


/**
 * @brief Dummy converter which does nothing
 * 
 * @tparam T 
 */
template<typename T>
class NOPConverter : public  BaseConverter<T> {
    public:
        virtual void convert(T (*src)[2], size_t size) {};
};

/**
 * @brief Multiplies the values with the indicated factor adds the offset and clips at maxValue. To mute use a factor of 0.0!
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterScaler : public  BaseConverter<T> {
    public:
        ConverterScaler(float factor, T offset, T maxValue){
            this->factor = factor;
            this->maxValue = maxValue;
            this->offset = offset;
        }

        void convert(T (*src)[2], size_t size) {
            for (size_t j=0;j<size;j++){
                src[j][0] = (src[j][0] + offset) * factor;
                if (src[j][0]>maxValue){
                    src[j][0] = maxValue;
                } else if (src[j][0]<-maxValue){
                    src[j][0] = -maxValue;
                }
                src[j][1] = src[j][1] + offset * factor;
                if (src[j][1]>maxValue){
                    src[j][1] = maxValue;
                } else if (src[j][0]<-maxValue){
                    src[j][1] = -maxValue;
                }
            }
        }
    protected:
        float factor;
        T maxValue;
        T offset;
};

/**
 * @brief Makes sure that the avg of the signal is set to 0
 * 
 * @tparam T 
 */
template<typename T>
class ConverterAutoCenter : public  BaseConverter<T> {
    public:
        ConverterAutoCenter(){
        }

        void convert(T (*src)[2], size_t size) {
            setup(src, size);
            if (is_setup){
                for (size_t j=0; j<size; j++){
                    src[j][0] = src[j][0] - offset;
                    src[j][1] = src[j][1] - offset;
                }
            }
        }

    protected:
        T offset;
        float left;
        float right;
        bool is_setup = false;

        void setup(T (*src)[2], size_t size){
            if (!is_setup) {
                for (size_t j=0;j<size;j++){
                    left += src[j][0];
                    right += src[j][1];
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
            }
        }
};

/**
 * @brief Switches the left and right channel
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterSwitchLeftAndRight : public  BaseConverter<T> {
    public:
        ConverterSwitchLeftAndRight(){
        }
        void convert(T (*src)[2], size_t size) {
            for (size_t j=0;j<size;j++){
                src[j][1] = src[j][0];
                src[j][0] = src[j][1];
            }
        }
};


enum FillLeftAndRightStatus {Auto, LeftIsEmpty, RightIsEmpty};

/**
 * @brief Make sure that both channels contain any data
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */


template<typename T>
class ConverterFillLeftAndRight : public  BaseConverter<T> {
    public:
        ConverterFillLeftAndRight(FillLeftAndRightStatus config=Auto){
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

        void convert(T (*src)[2], size_t size) {
            setup(src, size);
            if (left_empty && !right_empty){
                for (size_t j=0;j<size;j++){
                    src[j][0] = src[j][1];
                }
            } else if (!left_empty && right_empty) {
                for (size_t j=0;j<size;j++){
                    src[j][1] = src[j][0];
                }
            }
        }

    private:
        bool is_setup = false;
        bool left_empty = true;
        bool right_empty = true; 

        void setup(T src[][2], size_t size) {
            if (!is_setup) {
                for (int j=0;j<size;j++) {
                    if (src[j][0]!=0) {
                        left_empty = false;
                        break;
                    }
                }
                for (int j=0;j<size;j++) {
                    if (src[j][1]!=0) {
                        right_empty = false;
                        break;
                    }
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
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterToInternalDACFormat : public  BaseConverter<T> {
    public:
        ConverterToInternalDACFormat(){
        }

        void convert(T (*src)[2], size_t size) {
            for (int i=0; i<size; i++) {
                src[i][0] = src[i][0] + 0x8000;
                src[i][1] = src[i][1] + 0x8000;
            }
        }
};

/**
 * @brief Combines multiple converters
 * 
 * @tparam T 
 */
template<typename T>
class MultiConverter : public BaseConverter<T> {
    public:
        MultiConverter(){
        }

        // adds a converter
        void add(BaseConverter<T> &converter){
            converters.push_back(converter);
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(T (*src)[2], size_t size) {
            for(int i=0; i < converters.size(); i++){
                converters[i].convert(src);
            }
        }

    private:
        Vector<T> converters;

};

/**
 * @brief Converts e.g. 24bit data to the indicated bigger data type
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template<typename FromType, typename ToType>
class CallbackConverter {
    public:
        CallbackConverter(ToType (*converter)(FromType v)){
            this->convert_ptr = converter;
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(FromType src[][2], ToType target[][2], size_t size) {
            for (int i=size; i>0; i--) {
                target[i][0] = (*convert_ptr)(src[i][0]);
                target[i][1] = (*convert_ptr)(src[i][1]);
            }
        }

    private:
        ToType (*convert_ptr)(FromType v);

};


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
            int32_t result = static_cast<float>(value) / maxValue(inBits) * maxValue(outBits);
            if (!outSigned){
                result += (maxValue(outBits) / 2);
            }
            return result;
        }

};


}