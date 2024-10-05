#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "SoundGenerator.h"
#include "AudioEffect.h"
#if defined(USE_VARIANTS) && __cplusplus >= 201703L 
#  include <variant>
#endif

/** 
 * @defgroup effects Effects
 * @ingroup dsp
 * @brief Audio Effects  
**/

namespace audio_tools {

/**
 * Common functionality for managing a collection of effects
 * @author Phil Schatzmann
 * @copyright GPLv3
*/

class AudioEffectCommon {
    public:
        /// Adds an effect object (by reference)
        void addEffect(AudioEffect &effect){
            TRACED();
            effects.push_back(&effect);
        }

        /// Adds an effect using a pointer
        void addEffect(AudioEffect *effect){
            TRACED();
            effects.push_back(effect);
            LOGI("addEffect -> Number of effects: %d", (int)size());
        }
        /// deletes all defined effects
        void clear() {
            TRACED();
            effects.clear();
        }

        /// Provides the actual number of defined effects
        size_t size() {
            return effects.size();
        }

        /// Finds an effect by id
        AudioEffect* findEffect(int id){
            // find AudioEffect
            AudioEffect* result = nullptr;
            for (int j=0;j<size();j++){
                // we assume that ADSRGain is the first effect!
                if (effects[j]->id()==id){
                    result = effects[j];
                }
                LOGI("--> findEffect -> %d", effects[j]->id());
            }
            return result;
        }
        /// gets an effect by index
        AudioEffect* operator [](int idx){
            return effects[idx];
        }

    protected:
        Vector<AudioEffect*> effects;

};

/**
 * @brief OBSOLETE AudioEffects: the template class describes the input audio to which the effects are applied: 
 * e.g. SineWaveGenerator, SquareWaveGenerator, GeneratorFromStream etc. 
 * We support only one channel of int16_t data!
 * 
 * We subclass the AudioEffects from GeneratorT so that we can use this class with the GeneratedSoundStream 
 * class to output the audio.
 *   
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */


template <class GeneratorT>
class AudioEffects : public SoundGenerator<effect_t> {
    public:
        /// Default constructor
        AudioEffects() = default;

        /// Copy constructor
        AudioEffects(AudioEffects &copy) {
            TRACEI();
            // create a copy of the source and all effects
            p_generator = copy.p_generator;
            for (int j=0;j<copy.size();j++){
                effects.addEffect(copy[j]->clone());
            }
            LOGI("Number of effects %d -> %d", copy.size(), this->size());
        }

        /// Constructor which is assigning a generator
        AudioEffects(GeneratorT &generator) {
            setInput(generator);
        }

        /// Constructor which is assigning a Stream as input. The stream must consist of int16_t values 
        /// with the indicated number of channels. Type type parameter is e.g.  <GeneratorFromStream<effect_t>
        AudioEffects(Stream &input, int channels=2, float volume=1.0) {
            setInput(* (new GeneratorT(input, channels, volume)));
            owns_generator = true;
        }

        /// Destructor
        virtual ~AudioEffects(){
            TRACED();
            if (owns_generator && p_generator!=nullptr){
                delete p_generator;
            }
            for (int j=0;j<effects.size();j++){
                delete effects[j];
            }
        }
        
        /// Defines the input source for the raw guitar input
        void setInput(GeneratorT &in){
            TRACED();
            p_generator = &in;
            // automatically activate this object
            AudioInfo info;
            info.channels = 1;
            info.bits_per_sample = sizeof(effect_t)*8;
            begin(info);
        }

        /// Adds an effect object (by reference)
        void addEffect(AudioEffect &effect){
            TRACED();
            effects.addEffect(&effect);
        }

        /// Adds an effect using a pointer
        void addEffect(AudioEffect *effect){
            TRACED();
            effects.addEffect(effect);
            LOGI("addEffect -> Number of effects: %d", size());
        }

        /// provides the resulting sample
        effect_t readSample() override {
            effect_t sample;
            if (p_generator!=nullptr){
                sample  = p_generator->readSample();
                int size = effects.size();
                for (int j=0; j<size; j++){
                    sample = effects[j]->process(sample);
                }
            }
            return sample;
        }

        /// deletes all defined effects
        void clear() {
            TRACED();
            effects.clear();
        }

        /// Provides the actual number of defined effects
        size_t size() {
            return effects.size();
        }

        /// Provides access to the sound generator
        GeneratorT &generator(){
            return *p_generator;
        }

        /// gets an effect by index
        AudioEffect* operator [](int idx){
            return effects[idx];
        }

        /// Finds an effect by id
        AudioEffect* findEffect(int id){
            return effects.findEffect(id);
        }

    protected:
        AudioEffectCommon effects;
        GeneratorT *p_generator=nullptr;
        bool owns_generator = false;
};


/**
 * @brief EffectsStreamT: the template class describes an input or output stream to which one or multiple 
 * effects are applied. The number of channels are used to merge the samples of one frame into one sample
 * before outputting the result as a frame (by repeating the result sample for each channel).
 * Currently only int16_t values are supported, so I recommend to use the __AudioEffectStream__ class which is defined as 
 * using AudioEffectStream = AudioEffectStreamT<effect_t>;
  
 * @ingroup effects transform
 * @author Phil Schatzmann
 * @copyright GPLv3*/

template <class T>
class AudioEffectStreamT : public ModifyingStream {
  public:
    AudioEffectStreamT() = default;

    AudioEffectStreamT(Stream &io){
        setStream(io);
    }

    AudioEffectStreamT(Print &out){
        setOutput(out);
    }

    AudioInfo defaultConfig() {
        AudioInfo cfg;
        cfg.sample_rate = 44100;
        cfg.bits_per_sample = 16;
        cfg.channels = 2;
        return cfg;
    }

    bool begin(AudioInfo cfg){
        info = cfg;
        return begin();
    }

    bool begin(){
        TRACEI();
        if (sizeof(T)==info.bits_per_sample/8){
            active = true;
        } else {
            LOGE("bits_per_sample not consistent: %d",info.bits_per_sample);
            active = false;
        }
        return active;
    }

    void end() override {
        active = false;
    }

    void setStream(Stream &io) override {
        p_io = &io;
        p_print = &io;
    }


    void setOutput(Print &print) override {
        p_print = &print;
    }

    /**
     * Provides the audio data by reading the assinged Stream and applying
     * the effects on that input
    */
    size_t readBytes(uint8_t *data, size_t len) override {
        if (!active || p_io==nullptr)return 0;
        size_t result_size = 0;

        // read data from source
        size_t result = p_io->readBytes((uint8_t*)data, len);
        int frames = result / sizeof(T) / info.channels;
        T* samples = (T*) data;

        for (int count=0;count<frames;count++){
            // determine sample by combining all channels in frame
            T result_sample = 0;
            for (int ch=0;ch<info.channels;ch++){
                T sample = *samples++;
                //p_io->readBytes((uint8_t*)&sample, sizeof(T));
                result_sample += sample / info.channels;
            }

            // apply effects
            for (int j=0; j<size(); j++){
                result_sample = effects[j]->process(result_sample);
            }

            // write result multiplying channels 
            T* p_buffer = ((T*)data)+(count*info.channels);
            for (int ch=0;ch<info.channels;ch++){
                p_buffer[ch] = result_sample;
                result_size += sizeof(T);
            }
        }
        return result_size;
    }

    /**
     * Writes the samples passed in the buffer and applies the effects before writing the
     * result to the output defined in the constructor.
    */
    size_t write(const uint8_t *data, size_t len) override {
        if (!active)return 0;
        // length must be multple of channels
        assert(len % (sizeof(T)*info.channels)==0);
        int frames = len / sizeof(T) / info.channels;
        size_t result_size = 0;

        // process all samples
        for (int j=0;j<frames;j++){
            // calculate sample for frame
            T* p_buffer = ((T*)data) + (j*info.channels);
            T sample=0;
            for (int ch=0;ch<info.channels;ch++){
                sample += p_buffer[ch] / info.channels;
                result_size += sizeof(T);
            }

            // apply effects
            for (int j=0; j<size(); j++){
                sample = effects[j]->process(sample);
            }

            // wite result channel times to output defined in constructor
            for (int ch=0;ch<info.channels;ch++){
                if (p_io!=nullptr){
                    p_io->write((uint8_t*)&sample, sizeof(T));

                } else if (p_print!=nullptr){
                    p_print->write((uint8_t*)&sample, sizeof(T));
                }
            }
        }
        return result_size;
    }

    int available() override {
        if (p_io==nullptr) return 0;
        return p_io->available();
    }

    int availableForWrite() override {
        if (p_print!=nullptr) return p_print->availableForWrite();
        if (p_io!=nullptr) return p_io->availableForWrite();
        return 0;
    }

    /// Adds an effect object (by reference)
    void addEffect(AudioEffect &effect){
        TRACED();
        effects.addEffect(&effect);
    }

    /// Adds an effect using a pointer
    void addEffect(AudioEffect *effect){
        TRACED();
        effects.addEffect(effect);
        LOGI("addEffect -> Number of effects: %d", (int) size());
    }

    /// deletes all defined effects
    void clear() {
        TRACED();
        effects.clear();
    }

    /// Provides the actual number of defined effects
    size_t size() {
        return effects.size();
    }

    /// gets an effect by index
    AudioEffect* operator [](int idx){
        return effects[idx];
    }

    /// Finds an effect by id
    AudioEffect* findEffect(int id){
        return effects.findEffect(id);
    }

  protected:
    AudioEffectCommon effects;
    bool active = false;
    Stream *p_io=nullptr;
    Print *p_print=nullptr;
};

#if defined(USE_VARIANTS) && __cplusplus >= 201703L || defined(DOXYGEN)
/** 
 * @brief EffectsStream supporting variable bits_per_sample.
 * This class is only available when __cplusplus >= 201703L. Otherwise AudioEffectStream results in
 * using AudioEffectStream = AudioEffectStreamT<effect_t>;

 * @ingroup effects transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

class AudioEffectStream : public ModifyingStream {
    AudioEffectStream() = default;

    AudioEffectStream(Stream &io){
        setOutput(io);
        setInput(io);
    }

    AudioEffectStream(Print &out){
        setOutput(out);
    }

    AudioInfo defaultConfig() {
        AudioInfo cfg;
        cfg.sample_rate = 44100;
        cfg.bits_per_sample = 16;
        cfg.channels = 2;
        return cfg;
    }

    bool begin(AudioInfo cfg){
        info = cfg;
        return begin();
    }

    bool begin(){
        TRACEI();
        switch(info.bits_per_sample){
            case 16: 
                variant.emplace<0>();
                break;
            case 24: 
                variant.emplace<1>();
                break;
            case 32: 
                variant.emplace<2>();
                break;
            default:
                LOGE("Unspported bits_per_sample: %d", cfg.bits_per_sample);
                return false;
        }
        std::visit( [this](auto&& e) {return e.setOutput(*p_print);}, variant );
        std::visit( [this](auto&& e) {return e.setInput(*p_io);}, variant );
        return std::visit( [cfg](auto&& e) {return e.begin(info);}, variant );
    }

    void end() override {
        std::visit( [](auto&& e) {e.end();}, variant );
    }

    void setInput(Stream &io){
        setStream(io);
    }

    void setStream(Stream &io){
        p_io = &io;
    }

    void setOutput(Stream &io){
        p_io = &io;
    }

    void setOutput(Print &print){
        p_print = &print;
    }

    /**
     * Provides the audio data by reading the assinged Stream and applying
     * the effects on that input
    */
    size_t readBytes(uint8_t *data, size_t len) override {
        return std::visit( [data, len](auto&& e) {return e.readBytes(data, len);}, variant );
    }

    /**
     * Writes the samples passed in the buffer and applies the effects before writing the
     * result to the output defined in the constructor.
    */
    size_t write(const uint8_t *data, size_t len) override {
        return std::visit( [data, len](auto&& e) {return e.write(data, len);}, variant );
    }

    int available() override {
        return std::visit( [](auto&& e) {return e.available();}, variant );
    }

    int availableForWrite() override {
        return std::visit( [](auto&& e) {return e.availableForWrite();}, variant );
    }

    /// Adds an effect object (by reference)
    void addEffect(AudioEffect &effect){
        addEffect(&effect);
    }

    /// Adds an effect using a pointer
    void addEffect(AudioEffect *effect){
        std::visit( [effect](auto&& e) {e.addEffect(effect);}, variant );
    }

    /// deletes all defined effects
    void clear() {
        std::visit( [](auto&& e) {e.clear();}, variant );
    }

    /// Provides the actual number of defined effects
    size_t size() {
        return std::visit( [](auto&& e) {return e.size();}, variant );
    }

    /// gets an effect by index
    AudioEffect* operator [](int idx){
        return std::visit( [idx](auto&& e) {return e[idx];}, variant );
    }

    /// Finds an effect by id
    AudioEffect* findEffect(int id){
        return std::visit( [id](auto&& e) {return e.findEffect(id);}, variant );
    }

  protected:
    std::variant<AudioEffectStreamT<int16_t>, AudioEffectStreamT<int24_t>,AudioEffectStreamT<int32_t>> variant;
    Stream *p_io=nullptr;
    Print *p_print=nullptr;

};

#else
// Use int16_t as only supported data type
using AudioEffectStream = AudioEffectStreamT<effect_t>;
#endif

} // namespace