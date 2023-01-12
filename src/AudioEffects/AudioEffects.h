#pragma once

#include "AudioBasic/Collections.h"
#include "AudioEffects/SoundGenerator.h"
#include "AudioEffects/AudioEffect.h"
#include "AudioTools/AudioStreams.h"

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
            AudioBaseInfo info;
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
class AudioEffectStreamT : public AudioStreamX {
  public:
    AudioEffectStreamT() = default;

    AudioEffectStreamT(Stream &io){
        setOutput(io);
        setInput(io);
    }

    AudioEffectStreamT(Print &out){
        setOutput(out);
    }

    AudioBaseInfo defaultConfig() {
        AudioBaseInfo cfg;
        cfg.sample_rate = 44100;
        cfg.bits_per_sample = 16;
        cfg.channels = 2;
        return cfg;
    }

    bool begin(AudioBaseInfo cfg){
        info = cfg;
        active = true;
        return true;
    }

    void end() override {
        active = false;
    }

    void setInput(Stream &io){
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
    size_t readBytes(uint8_t *buffer, size_t length) override {
        if (!active || p_io==nullptr)return 0;
        int frames = length / sizeof(T) / info.channels;
        size_t result_size = 0;

        if (p_io->available()<(sizeof(T)*info.channels)){
            return 0;
        }

        for (int count=0;count<frames;count++){
            // determine sample by combining all channels in frame
            T result_sample = 0;
            for (int ch=0;ch<info.channels;ch++){
                T sample;
                p_io->readBytes((uint8_t*)&sample, sizeof(T));
                result_sample += sample / info.channels;
            }

            // apply effects
            for (int j=0; j<size(); j++){
                result_sample = effects[j]->process(result_sample);
            }

            // write result multiplying channels 
            T* p_buffer = ((T*)buffer)+(count*info.channels);
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
    size_t write(const uint8_t *buffer, size_t length) override {
        if (!active)return 0;
        // length must be multple of channels
        assert(length % (sizeof(T)*info.channels)==0);
        int frames = length / sizeof(T) / info.channels;
        size_t result_size = 0;

        // process all samples
        for (int j=0;j<frames;j++){
            // calculate sample for frame
            T* p_buffer = ((T*)buffer) + (j*info.channels);
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
        LOGI("addEffect -> Number of effects: %d", size());
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

/** 
 * @brief EffectsStream using effect_t (=int16_t) samples
 * @ingroup effects transform
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

using AudioEffectStream = AudioEffectStreamT<effect_t>;

} // namespace