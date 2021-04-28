#pragma once
#include "Config.h"
#include "SoundTypes.h"
#include "BluetoothA2DPSource.h"

namespace sound_tools {


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


/**
 * @brief Abstract Base class for Filters
 * A filter is processing the data in the indicated array
 * @tparam T 
 */
template<typename T>
class BaseFilter {
    public:
        virtual void process(T (*src)[2], size_t size) = 0;
};


/**
 * @brief Multiplies the values with the indicated factor adds the offset and clips at maxValue. To mute use a factor of 0.0!
 * 
 * @tparam T 
 */
template<typename T>
class FilterScaler : public  BaseFilter<T> {
    public:
        FilterScaler(float factor, T offset, T maxValue){
            this->factor = factor;
            this->maxValue = maxValue;
            this->offset = offset;
        }

        void process(T (*src)[2], size_t size) {
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
 * @brief Switches the left and right channel
 * 
 * @tparam T 
 */
template<typename T>
class FilterSwitchLeftAndRight : public  BaseFilter<T> {
    public:
        FilterSwitchLeftAndRight(){
        }
        void process(T (*src)[2], size_t size) {
            for (size_t j=0;j<size;j++){
                src[j][1] = src[j][0];
                src[j][0] = src[j][1];
            }
        }
};

/**
 * @brief Make sure that both channels contain any data
 * 
 * @tparam T 
 */
template<typename T>
class FilterFillLeftAndRight : public  BaseFilter<T> {
    public:
        FilterFillLeftAndRight(){
        }
        void process(T (*src)[2], size_t size) {
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
 * 
 * @tparam T 
 */
template<typename T>
class FilterToInternalDACFormat : public  BaseFilter<T> {
    public:
        FilterToInternalDACFormat(){
        }

        void process(T (*src)[2], size_t size) {
            for (int i=0; i<size; i++) {
                src[i][0] = src[i][0] + 0x8000;
                src[i][1] = src[i][1] + 0x8000;
            }
        }
};




/**
 * @brief Converts e.g. 24bit data to the indicated bigger data type
 * 
 * @tparam T 
 */
template<typename FromType, typename ToType>
class Converter {
    public:
        Converter(ToType (*converter)(FromType v)){
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


// static int16[2][] toArray(Channel *c){
//     return (static_cast<(int16[2])*>(c));
// }

/**
 * @brief Covnerts the data from T src[][2] to a Channels array 
 * 
 * @tparam T 
 */

template<typename T>
class ChannelConverter {
    public:
        ChannelConverter( int16_t (*convert_ptr)(T from)){
            this->convert_ptr = convert_ptr;
        }

        // The data is provided as int24_t tgt[][2] but  returned as int24_t
        void convert(T src[][2], Channels* channels, size_t size) {
            for (int i=size; i>0; i--) {
                channels[i].channel1 = (*convert_ptr)(src[i][0]);
                channels[i].channel2 = (*convert_ptr)(src[i][1]);
            }
        }

    private:
        int16_t (*convert_ptr)(T from);

};


}