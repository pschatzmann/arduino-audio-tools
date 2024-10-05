#pragma once

#include "AudioConfig.h"
#include "AudioTools/CoreAudio/AudioEffects/AudioEffect.h"
#ifdef ESP32
#  include "freertos/FreeRTOS.h"
#endif
#include "StkAll.h"

namespace audio_tools {

/**
 * @brief The Synthesis ToolKit in C++ (STK) is a set of open source audio signal processing and algorithmic synthesis classes
 * written in the C++ programming language. You need to install https://github.com/pschatzmann/Arduino-STK  
 * 
 * You can find further informarmation in the original Readme of the STK Project
 *
 * Like many other sound libraries it originates from an University (Princeton) and can look back at a very long history: 
 * it was created in 1995. In the 90s the computers had limited processor power and memory available. 
 * In todays world we can get some cheap Microcontrollers, which provide almost the same capabilities.
 *
 * @ingroup generator
 * @tparam T 
 */

template <class StkCls, class T>
class STKGenerator : public SoundGenerator<T> {
    public:
        STKGenerator() = default;

        // Creates an STKGenerator for an instrument
        STKGenerator(StkCls &instrument) : SoundGenerator<T>() {
            this->p_instrument = &instrument;
        }

        void setInput(StkCls &instrument){
            this->p_instrument = &instrument;
        }

        /// provides the default configuration
        AudioInfo defaultConfig() {
            AudioInfo info;
            info.channels = 2;
            info.bits_per_sample = sizeof(T) * 8;
            info.sample_rate = stk::Stk::sampleRate();
            return info;
        }

        /// Starts the processing
        bool begin(AudioInfo cfg){
            TRACEI();
            cfg.logInfo();
            SoundGenerator<T>::begin(cfg);
            max_value = NumberConverter::maxValue(sizeof(T)*8);
            stk::Stk::setSampleRate(SoundGenerator<T>::info.sample_rate);
            return true;
        }

        /// Provides a single sample
        T readSample() {
            T result = 0;
            if (p_instrument!=nullptr) {
                result = p_instrument->tick()*max_value;
            }
            return result;
        }

    protected:
        StkCls *p_instrument=nullptr;
        T max_value;

};

/**
 * @brief STK Stream for Instrument or Voicer
 * @ingroup dsp
 */
template <class StkCls>
class STKStream : public GeneratedSoundStream<int16_t> {
    public:
        STKStream() {
            GeneratedSoundStream<int16_t>::setInput(generator);          
        };

        STKStream(StkCls &instrument){
            generator.setInput(instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }
        void setInput(StkCls &instrument){
            generator.setInput(instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }
        void setInput(StkCls *instrument){
            generator.setInput(*instrument);
            GeneratedSoundStream<int16_t>::setInput(generator);
        }

        AudioInfo defaultConfig() {
            AudioInfo info;
            info.channels = 1;
            info.bits_per_sample = 16;
            info.sample_rate = stk::Stk::sampleRate();
            return info;
        }

    protected:
        STKGenerator<StkCls,int16_t> generator;

};

/**
 * @brief Use any effect from the STK framework: e.g. Chorus, Echo, FreeVerb, JCRev,
 * PitShift... https://github.com/pschatzmann/Arduino-STK
 *
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKEffect : public AudioEffect {
public:
  STKEffect(stk::Effect &stkEffect) { p_effect = &stkEffect; }

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return p_effect->tick(value) * 32767.0;
  }

protected:
  stk::Effect *p_effect = nullptr;
};

/**
 * @brief Chorus Effect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKChorus : public AudioEffect, public stk::Chorus {
public:
  STKChorus(float baseDelay = 6000) : stk::Chorus(baseDelay) {}
  STKChorus(const STKChorus& copy) = default;

  AudioEffect* clone() override{
    return new STKChorus(*this);
  }

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::Chorus::tick(value) * 32767.0;
  }
};

/**
 * @brief Echo Effect
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKEcho : public AudioEffect, public stk::Echo {
public:
  STKEcho(unsigned long maximumDelay = (unsigned long)Stk::sampleRate())
      : stk::Echo(maximumDelay) {}
  STKEcho(const STKEcho& copy) = default;

  AudioEffect* clone() override{
    return new STKEcho(*this);
  }

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::Echo::tick(value) * 32767.0;
  }
};

/**
 * @brief Jezar at Dreampoint's FreeVerb, implemented in STK.
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKFreeVerb : public AudioEffect, public stk::FreeVerb {
public:
  STKFreeVerb() = default;
  STKFreeVerb(const STKFreeVerb& copy) = default;
  AudioEffect* clone() override{
    return new STKFreeVerb(*this);
  }
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::FreeVerb::tick(value) * 32767.0;
  }
};

/**
 * @brief John Chowning's reverberator class.
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKChowningReverb : public AudioEffect, public stk::JCRev {
public:
  STKChowningReverb() = default;
  STKChowningReverb(const STKChowningReverb& copy) = default;
  AudioEffect* clone() override{
    return new STKChowningReverb(*this);
  }

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::JCRev::tick(value) * 32767.0;
  }
};

/**
 * @brief CCRMA's NRev reverberator class.
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKNReverb : public AudioEffect, public stk::NRev {
public:
  STKNReverb(float t60 = 1.0) : NRev(t60) {}
  STKNReverb(const STKNReverb& copy) = default;
  AudioEffect* clone() override{
    return new STKNReverb(*this);
  }
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::NRev::tick(value) * 32767.0;
  }
};

/**
 * @brief Perry's simple reverberator class
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKPerryReverb : public AudioEffect, public stk::PRCRev {
public:
  STKPerryReverb(float t60 = 1.0) : PRCRev(t60) {}
  STKPerryReverb(const STKPerryReverb& copy) = default;
  AudioEffect* clone() override{
    return new STKPerryReverb(*this);
  }
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::PRCRev::tick(value) * 32767.0;
  }
};

/**
 * @brief Pitch shifter effect class based on the Lent algorithm
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKLentPitShift : public AudioEffect, public stk::LentPitShift {
public:
  STKLentPitShift(float periodRatio = 1.0, int tMax = 512)
      : stk::LentPitShift(periodRatio, tMax) {}
  STKLentPitShift(const STKLentPitShift& copy) = default;

  AudioEffect* clone() override{
    return new STKLentPitShift(*this);
  }
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::LentPitShift::tick(value) * 32767.0;
  }
};

/**
 * @brief Simple Pitch shifter effect class: This class implements a simple pitch 
 * shifter using a delay line.
 * @ingroup effects
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class STKPitShift : public AudioEffect, public stk::PitShift {
public:
  STKPitShift() = default;
  STKPitShift(const STKPitShift& copy) = default;

  AudioEffect* clone() override{
    return new STKPitShift(*this);
  }
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return stk::PitShift::tick(value) * 32767.0;
  }
};


}

