#pragma once

#include "AudioLogger.h"

namespace audio_tools {

/**
 * @brief Base class to define the abstract interface for the sound generating classes
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 * @tparam T 
 */
template <class T>
class SoundGenerator  {
    public:
        SoundGenerator() {
        }

        virtual ~SoundGenerator() {
            end();
        }

        /// Provides the samples into simple array - which represents 1 channel
        virtual size_t readSamples(T* data, size_t sampleCount=512){
            for (size_t j=0;j<sampleCount;j++){
                data[j] = readSample();
            }
            return sampleCount;
        }

        /// Provides the samples into a 2 channel array
        virtual size_t readSamples(T src[][2], size_t frameCount) {
            T tmp[frameCount];
            int len = readSamples(tmp, frameCount);
            for (int j=0;j<len;j++) {
                T value = tmp[j];
                src[j][1] = src[j][0] = value;
            }
            return frameCount;
        }

        /// Provides a single sample
        virtual  T readSample() = 0;


        /// Provides the data as byte array with the requested number of channels
        virtual size_t readBytes( uint8_t *buffer, size_t lengthBytes){
            LOGD("readBytes: %d", (int)lengthBytes);
            size_t result = 0;
            int ch = channels();
            int frame_size = sizeof(T) * ch;
            if (active){
                switch (ch){
                    case 1:
                        result = readSamples((T*) buffer, lengthBytes / frame_size) ;
                        break;
                    case 2:
                        result = readSamples((T(*)[2]) buffer, lengthBytes / frame_size);
                        break;
                    default:
                        LOGE( "SoundGenerator::readBytes -> number of channels %d is not supported (use 1 or 2)", ch);
                        result = 0;
                        break;
                }
            } else {
                if (!activeWarningIssued){
                    LOGE("SoundGenerator::readBytes -> inactive");
                }
                activeWarningIssued = true;
                // when inactive did not generate anything
                result = 0;
            }
            LOGD( "SoundGenerator::readBytes (channels: %d) %zu bytes -> %zu samples", ch, lengthBytes, result);
            return result * frame_size;
        }

        void begin() {
            LOGI("SoundGenerator::begin");
            active = true;
        }

        /// ends the processing
        void end(){
            LOGI("SoundGenerator::end");
            active = false;
        }

        bool isActive() {
            return active;
        }

        void setChannels(int ch){
            output_channels = ch;
        }

        int channels() {
            return output_channels;
        }


    protected:
        bool active = false;
        bool activeWarningIssued = false;
        int output_channels = 1;

};



/**
 * @brief Generates a Sound with the help of sin() function.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class SineWaveGenerator : public SoundGenerator<T>, public AudioBaseInfoDependent {
    public:
        // the scale defines the max value which is generated
        SineWaveGenerator(float amplitude = 32767.0, float phase = 0.0) {
            LOGD("SineWaveGenerator");
            m_amplitude = amplitude;
            m_phase = phase;
        }

        void begin() {
            begin(1, 44100, 0);
        }

        void begin(AudioBaseInfo info, uint16_t frequency=0){
            begin(info.channels, info.sample_rate, frequency);
        }

        void begin(uint16_t sample_rate, uint16_t frequency=0){
            begin(1, sample_rate, frequency);
        }

        void begin(int channels, uint16_t sample_rate, uint16_t frequency=0){
            LOGI("SineWaveGenerator::begin");
            this->setChannels(channels);
            this->m_frequency = frequency;
            this->m_sample_rate = sample_rate;
            this->m_deltaTime = 1.0 / m_sample_rate;
            SoundGenerator<T>::active = true;
            logStatus();
        }

        /// provides the AudioBaseInfo
        AudioBaseInfo audioInfo() {
            AudioBaseInfo baseInfo;
            baseInfo.sample_rate = m_sample_rate;
            baseInfo.channels = SoundGenerator<T>::channels();
            baseInfo.bits_per_sample = sizeof(T)*8;
            return baseInfo;
        }

        /// Defines the frequency - after the processing has been started
        void setFrequency(uint16_t frequency) {
            this->m_frequency = frequency;
        }

        /// Provides a single sample
        virtual T readSample() {
            float angle = double_Pi * m_frequency * m_time + m_phase;
            T result = m_amplitude * sin(angle);
            m_time += m_deltaTime;
            return result;
        }

    protected:
        uint16_t m_sample_rate = 0; 
        float m_frequency = 0;
        float m_time = 0.0;
        float m_amplitude = 1.0;  
        float m_deltaTime = 0.0;
        float m_phase = 0.0;
        float double_Pi = PI * 2.0;

        void logStatus() {
            LOGI( "channels: %d", this->channels() );
            LOGI( "frequency: %f", this->m_frequency );
            LOGI( "sample rate: %u", this->m_sample_rate );
            LOGI( "amplitude: %f", this->m_amplitude );
            LOGI( "active: %s", SoundGenerator<T>::active ? "true" : "false" );
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

        /// Provides a single sample
        T readSample() {
        	return  ((rand() % (static_cast<T>(2 * scale)) - scale)); // generate number between  -scale / scale
        }

    protected:
        double scale;

};

}
