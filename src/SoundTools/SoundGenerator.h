#pragma once

namespace sound_tools {

/**
 * @brief Base class to define the abstract interface for the sound generating classes
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template <class T>
class SoundGenerator  {
    virtual size_t readBytes(uint8_t *buffer, size_t bytes) = 0;

    virtual size_t readSamples(T* data, size_t size)=0;

    virtual size_t readSamples(T src[][2], size_t size) {
        T tmp[size];
        int len = readSamples(tmp, size);
        for (int j=0;j<len;j++) {
            T value = tmp[j];
            src[j][1] = src[j][0] = value;
        }
        return len;
    }

};

/**
 * @brief Generates a Sound with the help of sin() function.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class SineWaveGenerator : public SoundGenerator<T> {
    public:
        // the scale defines the max value which is generated
        SineWaveGenerator(double scale=1.0) {
            this->scale = scale;
        }

        void begin(uint16_t sample_rate=44100, uint16_t frequency=0){
            this->frequency = frequency;
            this->sample_rate = sample_rate;
        }

        void setFrequency(uint16_t frequency) {
            this->frequency = frequency;
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
			return readSamples((T*)buffer, length*sizeof(T)) * sizeof(T);
        }

        virtual size_t readSamples(T* data, size_t len=512){
            for (int j=0;j<len;j++){
                data[j] = readSample();;
            }
            return len;
        }
                
    protected:
        uint64_t sample_step;
        uint16_t frequency;
        double scale;
        uint64_t sample_rate;

        T readSample() {
            return sin(2.0 * PI * sample_step++ * frequency / sample_rate) * (double)scale;
        }

};


/**
 * @brief Generates a Sound with the help of rand() function.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class NoiseGenerator : public SoundGenerator<T> {
    public:
        // the scale defines the max value which is generated
        NoiseGenerator(double scale=1.0) {
            this->scale = scale;
        }

        void begin(){
        }

        virtual size_t readBytes(uint8_t *buffer, size_t length){
			return readSamples((T*)buffer, length*sizeof(T)) * sizeof(T);
        }

        virtual size_t readSamples(T* data, size_t len=512){
            for (int j=0;j<len;j++){
                data[j] = readSample();
            }
            return len;
        }
        
    protected:
        double scale;

        T readSample() {
        	return  ((rand() % (static_cast<T>(2 * scale)) - scale)); // generate number between  -scale / scale
        }

};

}
