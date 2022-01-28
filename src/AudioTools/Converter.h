#pragma once
#include "AudioTypes.h"
#include "AudioCommon/Vector.h"

namespace audio_tools {

/**
 * @brief Converts from a source to a target number with a different type
 * 
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

};


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
            this->factor_value = factor;
            this->maxValue = maxValue;
            this->offset_value = offset;
        }

        void convert(T (*src)[2], size_t size) {
            for (size_t j=0;j<size;j++){
                src[j][0] = (src[j][0] + offset_value) * factor_value;
                if (src[j][0]>maxValue){
                    src[j][0] = maxValue;
                } else if (src[j][0]<-maxValue){
                    src[j][0] = -maxValue;
                }
                src[j][1] = src[j][1] + offset_value * factor_value;
                if (src[j][1]>maxValue){
                    src[j][1] = maxValue;
                } else if (src[j][0]<-maxValue){
                    src[j][1] = -maxValue;
                }
            }
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
        float factor_value;
        T maxValue;
        T offset_value;
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
 * @brief Swap byte order
 * 
 * @tparam FromType 
 * @tparam ToType 
 */
template<typename T>
class ConverterSwapBytes : public BaseConverter<T> {
    public:
        ConverterSwapBytes(){
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(T (*src)[2], size_t size) override {
            switch(sizeof(T)){
                case 2:
                    convert16(src,size);
                    break;
                case 3:
                    convert24(src,size);
                    break;
                case 4:
                    convert32(src,size);
                    break;
            }
        }

    protected:

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert32(T (*src)[2], size_t size) {
            for(int i=0; i < size; i++){
                src[i][0] = swap32(src[i][0]);
                src[i][1] = swap32(src[i][1]);
            }
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert24(T (*src)[2], size_t size) {
            for(int i=0; i < size; i++){
                src[i][0] = swap24(src[i][0]);
                src[i][1] = swap24(src[i][1]);
            }
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert16(T (*src)[2], size_t size) {
            for(int i=0; i < size; i++){
                src[i][0] = swap16(src[i][0]);
                src[i][1] = swap16(src[i][1]);
            }
        }

        int32_t swap32(int32_t value) {
            return ((value>>24)&0xff) | // move byte 3 to byte 0
                ((value<<8)&0xff0000) | // move byte 1 to byte 2
                ((value>>8)&0xff00) | // move byte 2 to byte 1
                ((value<<24)&0xff000000); // byte 0 to byte 3
        }

        int24_t swap24(int24_t value) {
            return ((value>>16)&0xff) | // move byte 2 to byte 0
                ((value)&0xff00) | // keep byte 2 
                ((value<<16)&0xff0000); // byte 0 to byte 2
        }

        int32_t swap16(int16_t value) {
            return (value>>8) | (value<<8);
        }
};

/**
 * @brief Filter out unexpected values. We store the last 3 samples and if the
 * 2nd sample is an outlier we replace it with the avarage of sample 1 and 3
 * 
 * @tparam T 
 */
template<typename T>
class ConverterOutlierFilter : public BaseConverter<T> {
    public:
        ConverterOutlierFilter(uint32_t correctionLimit=100000000){
            this->correctionLimit = correctionLimit;
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        virtual void convert(T (*src)[2], size_t size) override {
            for (int j=0;j<size;j++){
                processFrame(src[j]);
            }
        }

    protected:
        T last[3][2];
        int8_t lastCount = 0;
        uint32_t correctionLimit;
        
        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void processFrame(T src[2]) {
            if (lastCount<3){
                // fill history buffer
                last[lastCount][0] = src[0];
                last[lastCount][1] = src[1];
                src[0] = 0;
                src[1] = 0;
                lastCount++;
            } else {
                // update history buffer
                last[0][0] = last[1][0];
                last[0][1] = last[1][1];                
                last[1][0] = last[2][0];
                last[1][1] = last[2][1];                
                last[2][0] = src[0];
                last[2][1] = src[1];

                // correct unexpected values
                if (abs(last[1][0]-last[0][0]) > correctionLimit ){
                    last[1][0] = (last[0][0] + last[2][0]) / 2;
                }
                // correct unexpected values
                if (abs(last[1][1]-last[0][1]) > correctionLimit ){
                    last[1][1] = (last[0][1] + last[2][1]) / 2;
                }

                src[0] = last[1][0]; 
                src[1] = last[1][1]; 
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

        MultiConverter(BaseConverter<T> &c1){
            add(c1);
        }

        MultiConverter(BaseConverter<T> &c1, BaseConverter<T> &c2){
            add(c1);
            add(c2);
        }

        MultiConverter(BaseConverter<T> &c1, BaseConverter<T> &c2, BaseConverter<T> &c3){
            add(c1);
            add(c2);
            add(c3);
        }

        // adds a converter
        void add(BaseConverter<T> &converter){
            converters.push_back(&converter);
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(T (*src)[2], size_t size) {
            for(int i=0; i < converters.size(); i++){
                converters[i]->convert(src, size);
            }
        }

    private:
        Vector<BaseConverter<T>*> converters;

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
            int32_t result = static_cast<float>(value) / NumberConverter::maxValue(inBits) * NumberConverter::maxValue(outBits);
            if (!outSigned){
                result += (NumberConverter::maxValue(outBits) / 2);
            }
            return result;
        }

};


}