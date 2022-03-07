#pragma once

#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioTypes.h"
#include "AudioBasic/Vector.h"

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
            info.bits_per_sample = sizeof(T)*8;
            info.channels = 1;
            info.sample_rate = 44100;
        }

        virtual ~SoundGenerator() {
            end();
        }

        virtual void begin(AudioBaseInfo info) {
            this->info = info;
            begin();
        }

        virtual void begin() {
            LOGD(LOG_METHOD);
            active = true;
            activeWarningIssued = false;
            //info.bits_per_sample = sizeof(T)*8;
        }

        /// ends the processing
        virtual void end(){
            active = false;
        }

        /// Checks if the begin method has been called - after end() isActive returns false
        virtual bool isActive() {
            return active;
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
            //LOGD("readBytes: %d", (int)lengthBytes);
            size_t result = 0;
            int ch = audioInfo().channels;
            if (ch==0){
                LOGE("Undefine number of channels: %d",ch);
                ch = 1;
            }
            int frame_size = sizeof(T) * ch;
            if (active){
                int len = lengthBytes / frame_size;
                if (lengthBytes % frame_size!=0){
                    len++;
                }
                switch (ch){
                    case 1:
                        result = readSamples((T*) buffer, len) ;
                        break;
                    case 2:
                        result = readSamples((T(*)[2]) buffer, len);
                        break;
                    default:
                        LOGE( "SoundGenerator::readBytes -> number of channels %d is not supported (use 1 or 2)", ch);
                        result = 0;
                        break;
                }
            } else {
                if (!activeWarningIssued) {
                    LOGE("SoundGenerator::readBytes -> inactive");
                    activeWarningIssued=true;
                }
                result = 0;
            }
            //LOGD( "SoundGenerator::readBytes (channels: %d) %zu bytes -> %zu samples", ch, lengthBytes, result);
            return result * frame_size;
        }

        virtual AudioBaseInfo defaultConfig(){
            AudioBaseInfo def;
            def.bits_per_sample = sizeof(T)*8;
            def.channels = 1;
            def.sample_rate = 44100;
            return def;
        }

        virtual void setFrequency(uint16_t frequency) {
            LOGE("setFrequency not supported");
        }


        virtual AudioBaseInfo audioInfo() {
            return info;
        }

        virtual void setAudioInfo(AudioBaseInfo info){
            this->info = info;
            if (info.bits_per_sample!=sizeof(T)*8){
                LOGE("invalid bits_per_sample: %d", info.channels);
            }   
        }

    protected:
        bool active = false;
        bool activeWarningIssued = false;
        int output_channels = 1;
        AudioBaseInfo info;
        
};


/**
 * @brief Generates a Sound with the help of sin() function.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class SineWaveGenerator : public SoundGenerator<T>{
    public:

        // the scale defines the max value which is generated
        SineWaveGenerator(float amplitude = 32767.0, float phase = 0.0){
            LOGD("SineWaveGenerator");
            m_amplitude = amplitude;
            m_phase = phase;
        }

        void begin() override {
            LOGI(LOG_METHOD);
            SoundGenerator<T>::begin();
            this->m_deltaTime = 1.0 / SoundGenerator<T>::info.sample_rate;
        }

        void begin(AudioBaseInfo info) override {
            LOGI("%s::begin(channels=%d, sample_rate=%d)","SineWaveGenerator", info.channels, info.sample_rate);
            SoundGenerator<T>::begin(info);
            this->m_deltaTime = 1.0 / SoundGenerator<T>::info.sample_rate;
        }

        void begin(AudioBaseInfo info, uint16_t frequency){
            LOGI("%s::begin(channels=%d, sample_rate=%d, frequency=%d)","SineWaveGenerator",info.channels, info.sample_rate,frequency);
            SoundGenerator<T>::begin(info);
            this->m_deltaTime = 1.0 / SoundGenerator<T>::info.sample_rate;
            if (frequency>0){
                setFrequency(frequency);
            }
        }

        void begin(int channels, int sample_rate, uint16_t frequency=0){
            SoundGenerator<T>::info.channels  = channels;
            SoundGenerator<T>::info.sample_rate = sample_rate;
            begin(SoundGenerator<T>::info, frequency);
        }

        // update m_deltaTime
        virtual void setAudioInfo(AudioBaseInfo info) override {
            SoundGenerator<T>::setAudioInfo(info);
            this->m_deltaTime = 1.0 / SoundGenerator<T>::info.sample_rate;
        }

        virtual AudioBaseInfo defaultConfig() override {
            return SoundGenerator<T>::defaultConfig();
        }

        /// Defines the frequency - after the processing has been started
        void setFrequency(uint16_t frequency)  override {
            LOGI("setFrequency: %d", frequency);
            LOGI( "active: %s", SoundGenerator<T>::active ? "true" : "false" );
            m_frequency = frequency;
        }

        /// Provides a single sample
        virtual T readSample() override {
            float angle = double_Pi * m_frequency * m_time + m_phase;
            T result = m_amplitude * sin(angle);
            m_time += m_deltaTime;
            if (m_time > divisor) m_time -= divisor;
            return result;
        }

    protected:
        volatile float m_frequency = 0;
        float m_time = 0.0;
        float m_amplitude = 1.0;  
        float m_deltaTime = 0.0;
        float m_phase = 0.0;
        float double_Pi = PI * 2.0;
        float divisor = 1000000;


        void logStatus() {
            SoundGenerator<T>::info.logStatus();
            LOGI( "amplitude: %f", this->m_amplitude );
            LOGI( "active: %s", SoundGenerator<T>::active ? "true" : "false" );
        }

};

/**
 * @brief Generates a square wave sound.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class SquareWaveGenerator : public SineWaveGenerator<T> {
    public:
        SquareWaveGenerator(float amplitude = 32767.0, float phase = 0.0) : SineWaveGenerator<T>(amplitude, phase) {
            LOGD("SquareWaveGenerator");
        }

        virtual  T readSample() {
            return value(SineWaveGenerator<T>::readSample(), SineWaveGenerator<T>::m_amplitude);
        }

    protected:
        // returns amplitude for positive vales and -amplitude for negative values
        T value(T value, T amplitude) {
            return (value >= 0) ? amplitude : -amplitude;
        }
};


/**
 * @brief Generates a random noise sound with the help of rand() function.
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


/**
 * @brief Provides 0 as sound data. This can be used e.g. to test the output functionality which should optimally just output
 * silence and no artifacts.
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
template <class T>
class SilenceGenerator : public SoundGenerator<T> {
    public:
        // the scale defines the max value which is generated
        SilenceGenerator(double scale=1.0) {
            this->scale = scale;
        }

        /// Provides a single sample
        T readSample() {
            return  0; // return 0
        }

    protected:
        double scale;

};

/**
 * @brief An Adapter Class which lets you use any Stream as a Generator
 * 
 * @tparam T 
 */
template <class T>
class GeneratorFromStream : public SoundGenerator<T> {
    public:
        GeneratorFromStream() = default;

        GeneratorFromStream(Stream &input){
            setStream(input);
        }

        /// (Re-)Assigns a stream to the Adapter class
        void setStream(Stream &input){
            this->p_stream = &input;
        }
        
        /// Provides a single sample from the stream
        T readSample() {
            T data = 0;
            if (p_stream!=nullptr) {
                p_stream->readBytes((uint8_t*)&data, sizeof(T));
            }
            return data;
        }

    protected:
        Stream *p_stream = nullptr;
};

/**
 * @brief We generate the samples from an array which is provided in the constructor
 * 
 * @tparam T 
 */
template <class T>
class GeneratorFromArray : public SoundGenerator<T> {
  public:

    template  <size_t arrayLen> 
    GeneratorFromArray(T(&array)[arrayLen], int repeat=0) {
        LOGD(LOG_METHOD);
        this->maxRepeat = repeat;
        setArray(array, arrayLen);
    }

    template  <int arrayLen> 
    void setArray(T(&array)[arrayLen]){
        LOGD(LOG_METHOD);
        setArray(array, arrayLen);
    }

    void setArray(T*array, size_t size){
      this->tableLength = size;
      this->table = array;
      LOGI("tableLength: %d", (int)size);
    }

    /// Starts the generation of samples
    void begin() override {
      LOGI(LOG_METHOD);
      SoundGenerator<T>::begin();
      soundIndex = 0;
      repeatCounter = 0;
    }

    /// Provides a single sample
    T readSample() override {
      // at end deactivate output
      if (soundIndex >= tableLength) {
        // LOGD("reset index - soundIndex: %d, tableLength: %d",soundIndex,tableLength);
        soundIndex = 0;
        // deactivate when count has been used up
        if (maxRepeat>=1 && ++repeatCounter>=maxRepeat){
            this->active = false;
            LOGD("active: false");
        }
      }

      //LOGD("index: %d - active: %d", soundIndex, this->active);
      T result = 0;
      if (this->active) {
        result = table[soundIndex];
        soundIndex++;
      }

      return result;
    }


  protected:
    int soundIndex = 0;
    int maxRepeat = 0;
    int repeatCounter = 0;
    T *table;
    size_t tableLength = 0;

};


/**
 * @brief Mixer which combines multiple sound generators into one output
 * 
 * @tparam T 
 */
template <class T>
class GeneratorMixer : public SoundGenerator<T> {
    public:
        GeneratorMixer() = default;

        void add(SoundGenerator<T> &generator){
            vector.push_back(&generator);
        }
        void add(SoundGenerator<T> *generator){
            vector.push_back(generator);
        }

        void clear() {
            vector.clear();
        }

        T readSample() {
            T result;
            int count = 0;
            for (int j=0;j<vector.size();j++){
                T tmp = vector[j]->readSample();
                if (j==actualChannel){
                    result = tmp;
                }
            }
            actualChannel++;
            if (actualChannel>=vector.size()){
                actualChannel = 0;
            }
            return result;;
        }
    protected:
        Vector<SoundGenerator<T>*> vector;
        int actualChannel=0;

};



}
