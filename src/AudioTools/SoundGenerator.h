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
        /// Provides the samples into simple array - which represents 1 channel
        virtual size_t readSamples(T* data, size_t sampleCount=512){
            for (int j=0;j<sampleCount;j++){
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


        /// Provides the data as byte array
        virtual size_t readBytes( uint8_t *buffer, size_t lengthBytes, uint8_t channels=1){
            size_t result = 0;
            int frame_size = sizeof(T) * channels;
            if (active){
                switch (channels){
                    case 1:
                        result = readSamples((T*) buffer, lengthBytes / frame_size) ;
                        break;
                    case 2:
                        result = readSamples((T(*)[2]) buffer, lengthBytes / frame_size);
                        break;
                    default:
                        logger().printf(AudioLogger::Error, "SoundGenerator::readBytes -> number of channels %d is not supported (use 1 or 2)\n", channels);
                        result = 0;
                        break;
                }
            } else {
                logger().debug("SoundGenerator::readBytes -> inactive");
                // when inactive did not generate anything
                result = 0;
            }
            logger().printf(AudioLogger::Debug, "SoundGenerator::readBytes (channels: %d) %d -> %d\n",channels, lengthBytes,result);
            return result * frame_size;
        }

        void begin() {
            logger().info("SoundGenerator::begin");
            active = true;
        }

        /// ends the processing
        void end(){
            logger().info("SoundGenerator::end");
            active = false;
        }

        /// provides the logger
        AudioLogger &logger() {
            return AudioLogger::instance();
        }

        bool isActive() {
            return active;
        }

    protected:
        bool active = false;

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
        SineWaveGenerator(float amplitude = 32767.0, float phase = 0) {
            m_amplitude = amplitude;
            m_phase = phase;
        }

        /// Starts the processing by defining the sample rate and frequency
        void begin(uint16_t sample_rate=44100, uint16_t frequency=0){
            SoundGenerator<T>::logger().info("SineWaveGenerator::begin");
            this->m_frequency = frequency;
            this->m_sample_rate = sample_rate;
            this->m_deltaTime = 1.0 / m_sample_rate;
            SoundGenerator<T>::active = true;
            logStatus();
        }

        /// Defines the frequency - after the processing has been started
        void setFrequency(uint16_t frequency) {
            this->frequency = frequency;
        }

        /// Provides a single sample
        virtual T readSample() {
            float angle = double_Pi * m_frequency * m_time + m_phase;
            T result = m_amplitude * sin(angle);
            m_time += m_deltaTime;
            return result;
        }

    protected:
        uint16_t m_sample_rate; 

        float m_frequency = 0;
        float m_time = 0.0;
        float m_amplitude = 1.0;  
        float m_deltaTime = 1.0 / m_sample_rate;
        float m_phase = 0.0;
        float double_Pi = PI * 2.0;

        void logStatus() {
            AudioLogger &log = SoundGenerator<T>::logger();
            log.printf(AudioLogger::Info, "frequency: %f", this->m_frequency );
            log.printf(AudioLogger::Info, "sample rate: %u", this->m_sample_rate );
            log.printf(AudioLogger::Info, "amplitude: %f", this->m_amplitude );
            log.printf(AudioLogger::Info, "active: %s", SoundGenerator<T>::active ? "true" : "false" );
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
