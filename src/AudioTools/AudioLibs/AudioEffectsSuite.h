#pragma once

/**
 * A Suite of C++ Audio Effects Classes
 * Adapted from https://github.com/mhamilt/AudioEffectsSuite
 *
 * This implementation collects together a set of Audio DSP Effects with heavy
 * emphasis on modularity and class inheritance coded in a modern C++ style.
 * @author Matthew Hamilton
 * @copyright MIT License
 * 
 * Converted to Header Only
 * The original implementation is based on doubles. To save space, We are using floats. On a ESP32
 * - size of double: 8
 * - size of float: 4
 * @author Phil Schatzmann
 * 
 */

#include <cmath>
#include <cstdint>
#include <iostream>
#include "AudioTools/CoreAudio/AudioEffects/AudioEffect.h"
#include "AudioTools/CoreAudio/AudioBasic/soft_float_t.h"
#include "AudioTools/CoreAudio/AudioStreams.h"

#ifndef PI
#  define PI 3.141592653589793f
#endif

namespace audio_tools {

// soft_float_t: integer mantissa/exponent under the hood, so the whole
// suite's per-sample delay/filter/modulation arithmetic (the vast bulk of
// this file's float use) avoids the FPU on FPU-less MCUs. It keeps float's
// wide dynamic range (unlike q1_14_t's bounded +-2.0), which this file
// needs: delay-buffer indices run up to sampleRate, filter coefficients
// vary widely with cutoff. Setup-time-only transcendental calls (sin/cos/
// pow/sqrt/...) still go through soft_float_t's implicit float conversion,
// which is fine since they aren't in the per-sample hot path -- except
// EnvelopeFilter::processFloat(), which recomputes a full Chebyshev filter
// (multiple sin/cos/sqrt/log/pow calls) every sample; that's expensive on
// any platform and isn't something this typedef swap can fix.
//
// Every class below is templated on the numeric type it uses internally --
// named `effectsuite_t` (shadowing this default alias) so the class bodies
// below don't need renaming. This lets both SimpleLPF<float> and
// SimpleLPF<soft_float_t> (etc.) be instantiated side by side in the same
// program, e.g. to compare their output directly, rather than requiring two
// separate builds gated by PREFER_FIXEDPOINT. The macro only picks the
// *default* -- what you get when you don't specify a type explicitly.
#if PREFER_FIXEDPOINT
using effectsuite_t_default = soft_float_t;
#else
using effectsuite_t_default = float;
#endif

/**
 * @brief Base Class for Effects
 * @ingroup effects
 */
template <typename effectsuite_t = effectsuite_t_default>
class EffectSuiteBase  : public AudioEffect {
  /**
   * @brief Main process block for applying audio effect
   * @param inputSample The input audio sample for the effect to be applied to
   * @returns an audio sample as a effectsuite_t with effect applied
   */
  virtual effectsuite_t processFloat(effectsuite_t inputSample) = 0;

  /**
   * Main process block for applying audio effect
   * @param inputSample The int16_t input audio sample for the effect to be applied to
   * @returns an audio sample as a int16_t with effect applied
   */
  virtual effect_t process(effect_t inputSample) override {
    return this->active_flag ? (effect_t)(32767.0f * processFloat(static_cast<effectsuite_t>(inputSample)/32767.0f)) : inputSample;
  }

};


/**
 * @brief Class provides a wave table that can be populated with a number of
 * preallocated waveforms. These can be used to generate audio in themselves or
 * to modulate The parameters of another effect. Class initialised with sample
 * rate.
 * @author Matthew Hamilton
 * @ingroup effects
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class ModulationBaseClass {
public:
  /** Constructor */
  ModulationBaseClass() { srand(static_cast<unsigned>(time(0))); }

  // deep-copies waveTable (owned) -- the compiler-generated (shallow) copy
  // this replaces would leave clone and original pointing at the *same*
  // waveTable, which breaks AudioEffectStreamT's per-channel cloning for
  // stereo: each channel's clone must own independent state.
  ModulationBaseClass(ModulationBaseClass &copy) {
    sampleRate = copy.sampleRate;
    timeStep = copy.timeStep;
    tableIndex = copy.tableIndex;
    is_noise = copy.is_noise;
    memcpy(interpTable, copy.interpTable, sizeof(interpTable));
    if (copy.waveTable != nullptr) {
      waveTable = new effectsuite_t[sampleRate];
      std::copy(copy.waveTable, copy.waveTable + sampleRate, waveTable);
    }
  }

  ModulationBaseClass(effectsuite_t extSampRate) {
    this->sampleRate = extSampRate;
    timeStep = 1. / extSampRate;
    allocateMemory();
    // setInterpTable();
    srand(static_cast<unsigned>(time(0)));
  }
  /** Destructor */
  ~ModulationBaseClass() { delete[] waveTable; }

  /**
   * @brief setup the class with a given sample rate. Basically reperforming the
   * constructor
   * @param extSampRate External sample rate
   */
  void setupModulationBaseClass(effectsuite_t extSampRate) {
    if (waveTable != nullptr) {
      delete[] waveTable;
    }
    sampleRate = extSampRate;
    timeStep = 1. / extSampRate;
    allocateMemory();
  }

  /**
   * sets wavetable to one period of a triangle wave
   */
  void setTriangle() {
    std::fill(waveTable, waveTable + sampleRate, 0);
    const effectsuite_t radPerSec = 2 * 3.1415926536f * timeStep;
    for (int i = 0; i < sampleRate; i++) {
      for (int j = 0; j < 35; j += 1)
        waveTable[i] += pow(-1., j) *
                        (sin((2. * effectsuite_t(j) + 1) * i * radPerSec)) /
                        (2. * effectsuite_t(j) + 1);
    }
  }
  /**
   * sets wavetable to one period of a square wave
   */
  void setSquare() {
    std::fill(waveTable, waveTable + sampleRate, 0);
    const effectsuite_t radPerSec = 2 * 3.1415926536f * timeStep;
    for (int i = 0; i < sampleRate; i++) {
      for (int j = 0; j < 35; j += 1)
        waveTable[i] += (sin((2 * j + 1) * i * radPerSec)) / (2 * j + 1);
    }
  }
  /**
   * sets wavetable to one period of a sawtooth wave
   */
  void setSawtooth() {
    std::fill(waveTable, waveTable + sampleRate, 0);
    const effectsuite_t radPerSec = 2 * 3.1415926536f * timeStep;
    for (int i = 0; i < sampleRate; i++) {
      for (int j = 1; j < 11; j += 1)
        waveTable[i] += pow(-1, j) * sin(j * radPerSec * i) / effectsuite_t(j);
    }
  }
  /**
   * sets wavetable to one period of a sine wave oscillating between -1 and 1
   */
  void setSine() {
    const effectsuite_t radPerSec = 2 * 3.1415926536f * timeStep;
    for (int i = 0; i < sampleRate; i++)
      waveTable[i] = sin(i * radPerSec);
  }
  /**
   * sets wavetable to one period of a sine wave oscillating between 0 and 1
   */
  void setOffSine() {
    const effectsuite_t radPerSec = 2 * 3.1415926536f * timeStep;
    for (int i = 0; i < sampleRate; i++)
      waveTable[i] = (sin(i * radPerSec) + 1) * .5f;
  }

  void setNoise() {
    is_noise = true;
  }

  bool isNoise() {
    return is_noise;
  }

  /**
   * sets wavetable to DC one
   */
  void setDC() {
    for (int i = 0; i < sampleRate; i++)
      waveTable[i] = 1.0;
  }
  /** set wave table to be a ramp from 0 to 1 */
  void setRamp() {
    for (int i = 0; i < sampleRate; i++)
      waveTable[i] = i / effectsuite_t(sampleRate);
  }
  /**
   * reads out white noise
   * @return random number between -1 and 1
   */
  effectsuite_t readNoise() {
    const effectsuite_t lo = -1.;
    const effectsuite_t hi = 1.;
    return lo + static_cast<effectsuite_t>(rand()) /
                    (static_cast<effectsuite_t>(RAND_MAX / (hi - lo)));
  }
  /**
   * clip wave table values with a tanh function. Effect change with a variable
   * amp to control intensity.
   * @param amp amount to multiply signal before being fed through a tanh
   * function
   */
  void clipWave(effectsuite_t amp) {
    if (amp < .01) {
      amp = .01;
    }

    for (int i = 0; i < sampleRate; i++)
      waveTable[i] = tanh(amp * waveTable[i]) / tanh(amp);
  }

  /**
   * Read through values in waveTable as a given frequency
   * @param freq read speed in Hz: essentially the number of samples
   * jumped between reads
   * @return value from table as effectsuite_t
   */
  effectsuite_t readTable(effectsuite_t freq) {
    if (freq > 0) {
      //    const effectsuite_t out = getInterpOut(tableIndex);
      const effectsuite_t out = getSplineOut(tableIndex, int(freq));
      tableIndex += freq;
      if (tableIndex - sampleRate > 0)
        tableIndex -= sampleRate;

      return out;
    } else {
      return 0.;
    }
  }
  void printInterpTable() {
    for (int j = 0; j < res; j++) {
      for (int i = 0; i < order; i++) {
        std::cout << interpTable[i][j] << '\t';
      }
      std::cout << '\n';
    }
  }
  /**
   * populates the internal interpolation table
   * @return return tru on success, else false
   */
  bool setInterpTable() {
    effectsuite_t *polynomial_normaliser = new effectsuite_t[order];
    if (!polynomial_normaliser) {
      return false;
    }
    std::fill(polynomial_normaliser, polynomial_normaliser + order, 1);
    effectsuite_t *alphas = new effectsuite_t[res];
    if (!alphas) {
      return false;
    }

    for (int i = 0; i < res; i++) {
      alphas[i] = (i / float(res)) - 0.5;
    }

    effectsuite_t *anchors = new effectsuite_t[order];

    if ((order % 2) == 0) {
      for (int i = 0; i < order; i++) {
        anchors[i] = -(effectsuite_t(order) - 1) * 0.5 + effectsuite_t(i);
        std::fill(interpTable[i], interpTable[i] + res, 1);
      }
    } else {
      for (int i = 0; i < order; i++) {
        anchors[i] = (-(effectsuite_t(order)) * 0.5) + effectsuite_t(i);
      }
    }

    // loop for every value of alpha
    for (int q = 0; q < res; q++) {
      // loop for sub polynomial
      for (int j = 0; j < order; j++) {
        // loop for each point in subpoly
        for (int m = 0; m < order; m++) {
          if (m != j) {
            if (q == 0) {
              polynomial_normaliser[j] =
                  polynomial_normaliser[j] * (anchors[j] - anchors[m]);
            }
            interpTable[j][q] *= (alphas[q] - anchors[m]);
          }
        }
        interpTable[j][q] /= polynomial_normaliser[j];
      }
    }

    delete[] polynomial_normaliser;
    delete[] alphas;
    delete[] anchors;
    return true;
  }

protected:
  /**
   * allocate memory to internal wave table based on sample rate
   * @return returns true on success or false on failure
   */
  bool allocateMemory() {
    waveTable = new effectsuite_t[sampleRate];
    assert(waveTable!=nullptr);
    if (!waveTable) {
      return false;
    }
    std::fill(waveTable, waveTable + sampleRate, 0);
    return true;
  }
  /**
   * get the interpolated output of the waveTable from the
   * given buffer index
   * @param bufferIndex buffer index as effectsuite_t
   * @return returns interpolated value from surrounding wavtable indices
   */
  effectsuite_t getInterpOut(effectsuite_t bufferIndex) {
    const int order = 4;
    const int orderHalf = order * .5;
    const int res = 100;
    effectsuite_t interpOut = 0;
    int intBufferIndex = floor(bufferIndex);
    int alphaIndex = int(floor((bufferIndex - intBufferIndex) * res));

    for (int i = 0; i < order; i++) {
      int interpIndex =
          (((i + 1 - orderHalf) + intBufferIndex) + sampleRate) % sampleRate;
      interpOut += (interpTable[i][alphaIndex]) * (waveTable[interpIndex]);
    }
    return interpOut;
  }

  /**
   * get a cubic spline interpolated out from the wave table
   *
   *  Derived from code by Alec Wright at repository:
   *  https://github.com/Alec-Wright/Chorus
   *
   *  @authors Matthew Hamilton, Alec Wright
   *  @param bufferIndex the required buffer index
   *  @param freq (speed) that the table is being read through
   *  @return returns interpolated value as effectsuite_t
   */
  effectsuite_t getSplineOut(effectsuite_t bufferIndex, int freq) {
    if (freq < 1) {
      freq = 1;
    }
    const int n0 = floor(bufferIndex);
    const int n1 = (n0 + freq) % sampleRate;
    const int n2 = (n0 + (2 * freq)) % sampleRate;
    const effectsuite_t alpha = bufferIndex - n0;
    const effectsuite_t a = waveTable[n1];
    const effectsuite_t c = ((3 * (waveTable[n2] - waveTable[n1])) -
                      (3 * (waveTable[n1] - waveTable[n0]))) *
                     .25;
    const effectsuite_t b = (waveTable[n2] - waveTable[n1]) - ((2 * c)) / 3;
    const effectsuite_t d = (-c) / 3;
    return a + (b * alpha) + (c * alpha * alpha) + (d * alpha * alpha * alpha);
  }

public:
  /** current table read index */
  effectsuite_t tableIndex = 0;
  /** Internal Sample Rate */
  int sampleRate;
  /** time between samples: 1/sampRate */
  effectsuite_t timeStep;
  /** store modulation signal as*/
  // was uninitialized in the default (no-arg) constructor -- the new
  // destructor's delete[] would then free indeterminate memory
  effectsuite_t *waveTable = nullptr;

protected:
  static const int order = 4;
  static const int res = 100;
  effectsuite_t interpTable[order][res];// = {1};
  bool is_noise = false;
};

/**
 * @brief SoundGenerator using the ModulationBaseClass
 * to generate the samples.
 * @ingroup effects
 * @tparam T 
 */
 template <class T, typename effectsuite_t = effectsuite_t_default>
 class SoundGeneratorModulation : public SoundGenerator<T>{
    public:
      SoundGeneratorModulation(ModulationBaseClass<effectsuite_t> &mod, int freq){
        p_mod = &mod;
        this->freq = freq;
      }
      bool begin(AudioInfo info) override{
          max_value = pow(2, info.bits_per_sample)/2-1;
          return SoundGenerator<T>::begin(info);
      }
      virtual  T readSample() override {
        return p_mod->isNoise() ? max_value * p_mod->readNoise() : max_value * p_mod->readTable(freq);
      }

  protected:
    ModulationBaseClass<effectsuite_t> *p_mod=nullptr;
    int freq;
    float max_value=32767;
};


/**
 * @brief A Base class for delay based digital effects. Provides the basic methods
 * that are shared amongst Flanger, Delay, Chorus and Phaser
 * @version 0.1
 * @see DelayEffectBase
 * @author Matthew Hamilton
 * @ingroup effects
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class DelayEffectBase  {
public:
  /** Constructor. */
  DelayEffectBase() = default;

  // deep-copies delayBuffer (owned) -- the compiler-generated (shallow) copy
  // this replaces would leave clone and original pointing at the *same*
  // delayBuffer, which breaks AudioEffectStreamT's per-channel cloning for
  // stereo (and would double-free delayBuffer when both are destroyed,
  // since ~DelayEffectBase() below does free it). interpolationTable is a
  // shared *static* lookup table -- one per DelayEffectBase<effectsuite_t>
  // instantiation, not per-instance state -- so it's correctly left alone
  // here.
  DelayEffectBase(DelayEffectBase &copy) {
    maxDelayBufferSize = copy.maxDelayBufferSize;
    delayTimeSamples = copy.delayTimeSamples;
    currentDelayWriteIndex = copy.currentDelayWriteIndex;
    currentDelayReadIndex = copy.currentDelayReadIndex;
    error = copy.error;
    if (copy.delayBuffer != nullptr) {
      delayBuffer = new effectsuite_t[maxDelayBufferSize];
      std::copy(copy.delayBuffer, copy.delayBuffer + maxDelayBufferSize, delayBuffer);
    }
  }

  DelayEffectBase(int bufferSizeSamples) {
    error = setDelayBuffer(bufferSizeSamples);
    delayTimeSamples = bufferSizeSamples;
    if (interpolationTable == nullptr) {
      interpolationTable = DelayEffectBase::setInterpolationTable();
    }
  }

  /** Destructor. */
  ~DelayEffectBase() {
    if (delayBuffer != nullptr)
      delete[] delayBuffer;
  }

  /**
   * <#Description#>
   * @param bufferSizeSamples <#bufferSizeSamples description#>
   */
  void setupDelayEffectBase(const int bufferSizeSamples) {
    error = setDelayBuffer(bufferSizeSamples);
    delayTimeSamples = bufferSizeSamples;
  }

protected:
  /**
   * Sets the internal lagrange interpolation table. Ideally it should be
   * shared amongst all
   *
   * @returns    false and an error description if there's a problem,
   * or sets the interpolation lookup table and returns true
   */
  static effectsuite_t **setInterpolationTable() {
    const int order = interpOrder;
    const int res = interpResolution;
    effectsuite_t **interpTable = new effectsuite_t *[order];
    if (!interpTable) {
      return NULL;
    }

    for (int i = 0; i < order; i++) {
      interpTable[i] = new effectsuite_t[res + 1];
      if (!interpTable[i]) {
        return NULL;
      }
      std::fill(interpTable[i], interpTable[i] + res, 1);
    }

    effectsuite_t *polynomial_normaliser = new effectsuite_t[order];
    if (!polynomial_normaliser) {
      return NULL;
    }
    std::fill(polynomial_normaliser, polynomial_normaliser + order, 1);
    effectsuite_t *alphas = new effectsuite_t[res];
    if (!alphas) {
      return NULL;
    }

    for (int i = 0; i < res; i++) {
      alphas[i] = (i / float(res)) - 0.5;
    }

    effectsuite_t *anchors = new effectsuite_t[order];

    if ((order % 2) == 0) {
      for (int i = 0; i < order; i++) {
        anchors[i] = -(effectsuite_t(order) - 1) * 0.5 + effectsuite_t(i);
      }
    } else {
      for (int i = 0; i < order; i++) {
        anchors[i] = (-(effectsuite_t(order)) * 0.5) + effectsuite_t(i);
      }
    }

    // loop for every value of alpha
    for (int q = 0; q < res; q++) {
      // loop for sub polynomial
      for (int j = 0; j < order; j++) {
        // loop for each point in subpoly
        for (int m = 0; m < order; m++) {
          if (m != j) {
            if (q == 0) {
              polynomial_normaliser[j] =
                  polynomial_normaliser[j] * (anchors[j] - anchors[m]);
            }
            interpTable[j][q] *= (alphas[q] - anchors[m]);
          }
        }
        interpTable[j][q] /= polynomial_normaliser[j];
      }
    }
    delete[] polynomial_normaliser;
    delete[] alphas;
    delete[] anchors;
    return interpTable;
  }

  // void printInterpTable() {
  //   for (int j = 0; j < interpResolution; j++) {
  //     for (int i = 0; i < interpOrder; i++) {
  //       printf("index %d: %.2f \t", i, interpolationTable[i][j]);
  //     }
  //     printf("\n");
  //   }
  // }

protected:
  /**
   * Allocates memory for delay buffer and initialises all elements to 0
   * @param bufferSizeSamples		the size of delay buffer to be used
   * @returns   false and error description if there's a problem,
   * or sets the internal delay buffer and returns true
   */
  bool setDelayBuffer(int bufferSizeSamples) {
    maxDelayBufferSize = bufferSizeSamples;
    delayBuffer = new effectsuite_t[maxDelayBufferSize];
    if (!delayBuffer) {
      return false;
    }
    std::fill(delayBuffer, delayBuffer + maxDelayBufferSize, 0);
    return true;
  }

  /**
   * store input sample into the delay buffer
   * @param inputSample sample to be stored for delay (effectsuite_t)
   */
  void storeSample(effectsuite_t inputSample) {
    delayBuffer[currentDelayWriteIndex] = inputSample;
  }

  /**
   * Increments the currentDelayWriteBufferIndex by 1
   */
  void incDelayBuffWriteIndex() {
    currentDelayWriteIndex++;
    currentDelayWriteIndex %= delayTimeSamples;
  }

  /**
   * Increments the currentDelayBufferReadIndex by indexInc
   * @param indexInc The amount to increment the delay buffer index
   */
  void incDelayBuffReadIndex(effectsuite_t indexInc) {
    currentDelayReadIndex += indexInc;
    if (currentDelayReadIndex >= effectsuite_t(delayTimeSamples)) {
      currentDelayReadIndex = 0;
    }
    if (currentDelayReadIndex < 0) {
      currentDelayReadIndex = 0;
    }
  }

  /**
   * sets the currentDelayBufferReadIndex by indexInc (Currently no wrapping)
   * @param index the read index index required
   */
  void setDelayBuffReadIndex(effectsuite_t index) {
    currentDelayReadIndex = index;
    if (currentDelayReadIndex >= effectsuite_t(delayTimeSamples)) {
      currentDelayReadIndex = 0;
    }
    if (currentDelayReadIndex < 0) {
      currentDelayReadIndex = 0;
    }
  }

  /**
   * store input sample into the delay buffer and increment
   * currentDelayWriteIndex for tracking when to loop back to start of buffer
   * @param inputSample sample to be stored for delay (effectsuite_t)
   */
  void delaySample(effectsuite_t inputSample) {
    storeSample(inputSample);
    incDelayBuffWriteIndex();
  }
  /**
   * get the value of the requested buffer index by interpolating other points
   * @param bufferIndex	The required buffer index
   * @returns interpolated output
   */
  effectsuite_t getInterpolatedOut(effectsuite_t bufferIndex) {
    const int order = interpOrder;
    const int orderHalf = order * .5;
    const int res = interpResolution;
    effectsuite_t interpOut = 0;
    int intBufferIndex = floor(bufferIndex);
    int alphaIndex = int(floor((bufferIndex - intBufferIndex) * res));

    for (int i = 0; i < order; i++) {
      int interpIndex = (i + 1 - orderHalf) + intBufferIndex;
      if (interpIndex < 0 || (interpIndex >= maxDelayBufferSize)) {
        if (interpIndex < 0) {
          interpIndex = maxDelayBufferSize + interpIndex;
        } else {
          interpIndex = interpIndex - maxDelayBufferSize;
        }
      }

      interpOut +=
          (interpolationTable[i][alphaIndex]) * (delayBuffer[interpIndex]);
    }
    return interpOut;
  }

protected: // member variables
  /** buffer to stored audio buffer for delay effects*/
  effectsuite_t *delayBuffer = 0;
  /** Maximum number of samples that can be stored in delayBuffer*/
  int maxDelayBufferSize = 441000;
  /** the delay time of signal in samples*/
  int delayTimeSamples = 44100;
  int currentDelayWriteIndex = 0;
  effectsuite_t currentDelayReadIndex = 0;
  static const int interpOrder = 4;
  static const int interpResolution = 1000;
  /**
   * @brief Table of interpolation values as a 2D array indexed by
   * interpolationTable[pointIndex][alphaIndex]. One instance per
   * DelayEffectBase<effectsuite_t> instantiation (previously a single
   * namespace-scope global shared -- and type-mismatched -- across every
   * effectsuite_t choice).
   */
  static effectsuite_t **interpolationTable;

  /** internal class error boolean*/
  bool error;
};

template <typename effectsuite_t>
effectsuite_t **DelayEffectBase<effectsuite_t>::interpolationTable = nullptr;

/**
 * @brief A Base class for filter based effects including methods for simple
 * high, low and band pass filtering
 * @see FilterEffectBase
 * @author Matthew Hamilton
 * @ingroup effects
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class FilterEffectBase : public EffectSuiteBase<effectsuite_t>  {
public:
  /** Constructor. */
  FilterEffectBase() { std::fill(rmsBuffer, rmsBuffer + rmsWindowSize, 0); }

  // deep-copies the owned filter/rms buffers -- the compiler-generated
  // (shallow) copy this replaces would leave clone and original pointing
  // at the *same* buffers, which breaks AudioEffectStreamT's per-channel
  // cloning for stereo (each channel's IIR filter state must be
  // independent, or one channel's audio corrupts the other's).
  FilterEffectBase(FilterEffectBase &copy) {
    bufferIndex = copy.bufferIndex;
    filterOrder = copy.filterOrder;
    samplingRate = copy.samplingRate;
    rmsBufferIndex = copy.rmsBufferIndex;

    // firCoefficients/iirCoefficients/firBuffer/iirBuffer may be allocated
    // wider than filterOrder (allocateBufferMemory() allocates coefficient
    // arrays at a fixed 22), but applyFilter() and everything downstream of
    // setup only ever reads indices [0, filterOrder), so copying exactly
    // filterOrder elements is always enough to preserve behavior.
    if (filterOrder > 0 && copy.firCoefficients != nullptr) {
      firCoefficients = new float[filterOrder];
      std::copy(copy.firCoefficients, copy.firCoefficients + filterOrder, firCoefficients);
    }
    if (filterOrder > 0 && copy.iirCoefficients != nullptr) {
      iirCoefficients = new float[filterOrder];
      std::copy(copy.iirCoefficients, copy.iirCoefficients + filterOrder, iirCoefficients);
    }
    if (filterOrder > 0 && copy.firBuffer != nullptr) {
      firBuffer = new float[filterOrder];
      std::copy(copy.firBuffer, copy.firBuffer + filterOrder, firBuffer);
    }
    if (filterOrder > 0 && copy.iirBuffer != nullptr) {
      iirBuffer = new float[filterOrder];
      std::copy(copy.iirBuffer, copy.iirBuffer + filterOrder, iirBuffer);
    }
    // firTemp/iirTemp are pure scratch space used only transiently inside
    // setChebyICoefficients() (overwritten from firCoefficients/
    // iirCoefficients at the start of every use there), so the clone just
    // needs its own 22-element scratch buffers, not copies of the old
    // contents.
    if (copy.firTemp != nullptr) {
      firTemp = new float[22];
      std::fill(firTemp, firTemp + 22, 0);
    }
    if (copy.iirTemp != nullptr) {
      iirTemp = new float[22];
      std::fill(iirTemp, iirTemp + 22, 0);
    }

    rmsBuffer = new effectsuite_t[rmsWindowSize];
    std::copy(copy.rmsBuffer, copy.rmsBuffer + rmsWindowSize, rmsBuffer);
  }

  /** Destructor. */
  ~FilterEffectBase() {
    delete[] firCoefficients;
    delete[] iirCoefficients;
    delete[] firTemp;
    delete[] iirTemp;
    delete[] firBuffer;
    delete[] iirBuffer;
    delete[] rmsBuffer;
  }

  /**
   * with the current filter coefficients this method filters a
   * sample then stores it the sample Buffer and increments the index
   * @param sampVal is the sample to be processed
   * @returns filtered audio sample
   */
  virtual effectsuite_t applyFilter(effectsuite_t sampVal) {
    // native float in/out at this boundary; see the member declarations
    // below for why the IIR recursion itself stays float
    float outSample = 0;
    firBuffer[bufferIndex] = (float)sampVal;

    for (int j = 0; j < filterOrder; j++) {
      int i = ((bufferIndex - j) + filterOrder) % filterOrder;
      outSample +=
          firCoefficients[j] * firBuffer[i] + iirCoefficients[j] * iirBuffer[i];
    }

    iirBuffer[bufferIndex] = outSample;
    incBufferIndex();

    return outSample;
  }

  virtual effectsuite_t processFloat(effectsuite_t inputSample) override {
    return applyFilter(inputSample);
  }

  /// see applyFilter
  virtual effect_t process(effect_t inputSample) override {
    return this->active_flag ? (effect_t)(32767.0 * processFloat(static_cast<effectsuite_t>(inputSample)/32767.0)) : inputSample;
  }

  /**
   *  detect the envelop of an incoming signal
   * @param sample		the incoming signal sample value
   * @returns returns envelope dection signal sample value
   */
  effectsuite_t envelope(effectsuite_t sample) { return applyFilter(rms(sample)); }

  // void printBuffers() {
  //   printf("FIRb\t\tIIRb\n");
  //   for (int i = 0; i < filterOrder; i++) {
  //     printf("%.4e\t%.4e\n", firBuffer[i], iirBuffer[i]);
  //   }
  //   printf("\n");
  // }

  // void printCoefs() {
  //   printf("FIR\t\tIIR\n");
  //   for (int i = 0; i < filterOrder; i++) {
  //     printf("%.4e\t%.4e\n", firCoefficients[i], iirCoefficients[i]);
  //   }
  //   printf("\n");
  // }

  /** changes the current Chebyshev type 1 coefficients without altering the
   * filter order. This allows for use in an audio process thread as it avoids
   * dynamic allocation of memory. Filter sample and coefficient buffers are
   * unaltered
   * for initialising a chebyshev type 1 filter @see setChebyICoefficients
   * @params cutFreq		normalised cutoff frequency (0 < x < .5)
   * @params shelfType	bool filter shelf type, false = low pass, true = high
   * pass
   * @params ripple		percentage ripple (<.2929)
   * @returns boolean false on error and true on success
   */
  // Coefficient derivation chains dozens of transcendental calls and
  // differences of near-equal quantities -- but it only runs at
  // setup/reconfiguration time, never per-sample (EnvelopeFilter is the
  // one exception, and it's already documented as a separate, unresolved
  // hotspot), so it costs nothing to keep it in native float, matching
  // firCoefficients/iirCoefficients' own type (see their declarations for
  // why applyFilter()'s IIR recursion needs float precision).
  bool setChebyICoefficients(effectsuite_t cutFreqIn, bool shelfType, effectsuite_t rippleIn) {
    const float cutFreq = (float)cutFreqIn;
    const float ripple = (float)rippleIn;

    // NOTE: coefficient buffers must be cleared as are additive in the
    // following 		code
    std::fill(firCoefficients, firCoefficients + 22, 0);
    std::fill(iirCoefficients, iirCoefficients + 22, 0);

    float poles = (float)filterOrder - 1;
    int order = (int)poles;

    firCoefficients[2] = 1;
    iirCoefficients[2] = 1;

    float Es, Vx, Kx;
    if (ripple != 0) {
      Es = sqrt(pow(1 / (1 - ripple), 2) - 1);
      Vx = (1 / poles) * log(1 / Es + sqrt(1 / (pow(Es, 2)) + 1));
      Kx = (1 / poles) * log(1 / Es + sqrt(1 / (pow(Es, 2)) - 1));
      Kx = cosh(Kx);
    } else {
      Vx = 1;
      Kx = 1;
    }

    const float T = 2.0f * tan(.5f);
    const float W = 2.0f * (float)PI * cutFreq;

    float K;

    if (shelfType == 0) //// if low pass
    {
      K = sin(.5f - W / 2) / sin(.5f + W / 2);
    } else //// if high pass
    {

      K = -cos(.5f + W / 2) / cos(W / 2 - .5f);
    }

    ////// main algorithm
    for (int i = 0; i < (order / 2); i++) {
      ////// Sub routine
      const float alpha = (float)PI / (2 * poles) + (i - 1) * ((float)PI / poles);

      float Rp, Ip;
      if (ripple != 0) {
        Rp = -cos(alpha) * sinh(Vx) / Kx;
        Ip = sin(alpha) * cosh(Vx) / Kx;
      } else {
        Rp = -cos(alpha);
        Ip = sin(alpha);
      }

      const float M = pow(Rp, 2) + pow(Ip, 2);
      const float D = 4 - 4 * Rp * T + M * T;

      const float X0 = (pow(T, 2)) / D;
      const float X1 = (2 * pow(T, 2)) / D;
      const float X2 = X0;

      const float Y1 = (8 - (2 * M * pow(T, 2))) / D;
      const float Y2 = (-4 - 4 * Rp * T - M * T) / D;

      // renamed and inverted from original algorithm
      const float _D1 = 1 / (1 + Y1 * K - Y2 * pow(K, 2));

      const float _A0 = (X0 - X1 * K + X2 * pow(K, 2)) * _D1;
      float _A1 = (-2 * X0 * K + X1 + X1 * pow(K, 2) - 2 * X2 * K) * _D1;
      const float _A2 = (X0 * pow(K, 2) - X1 * K + X2) * _D1;

      float _B1 = (2 * K + Y1 + Y1 * pow(K, 2) - 2 * Y2 * K) * _D1;
      const float B2 = (-(pow(K, 2)) - Y1 * K + Y2) * _D1;

      if (shelfType == 1) {
        _A1 = -_A1;
        _B1 = -_B1;
      }

      for (int j = 0; j < 22; j++) {
        firTemp[j] = firCoefficients[j];
        iirTemp[j] = iirCoefficients[j];
      }
      for (int j = 2; j < 22; j++) {
        firCoefficients[j] =
            _A0 * firTemp[j] + _A1 * firTemp[j - 1] + _A2 * firTemp[j - 2];
        iirCoefficients[j] =
            iirTemp[j] - _B1 * iirTemp[j - 1] - B2 * iirTemp[j - 2];
      }
    } //// end for i

    iirCoefficients[2] = 0;
    for (int j = 0; j < filterOrder; j++) {
      firCoefficients[j] = firCoefficients[j + 2];
      iirCoefficients[j] = -iirCoefficients[j + 2];
    }
    //////// Normalising
    float SA = 0;
    float SB = 0;
    if (shelfType == 0) {
      for (int j = 0; j < filterOrder; j++) {
        SA += firCoefficients[j];
        SB += iirCoefficients[j];
      }
    } else {
      for (int j = 0; j < order; j++) {
        SA += firCoefficients[j] * pow(-1, j);
        SB += iirCoefficients[j] * pow(-1, j);
      }
    }

    const float gain = SA / (1 - SB);
    for (int j = 0; j < filterOrder; j++) {
      firCoefficients[j] /= gain;
    }

    return true;
  }

protected:
  /**
   * set firCoefficients and iirCoefficients for required chebyshev type I
   * filter sampleBuffer memory is also set
   * @params cutFreq		normalised cutoff frequency (0 < x < .5)
   * @params shelfType	bool filter shelf type, false = low pass, true = high
   *pass
   * @params ripple		percentage ripple (<.2929)
   * @params poles		number of poles
   * @returns boolean false on error and true on success
   */
  bool changeChebyICoefficients(effectsuite_t cutFreq, bool shelfType, effectsuite_t ripple,
                                int poles) {
    filterOrder = poles + 1;
    clearMemory();
    allocateBufferMemory();
    setChebyICoefficients(cutFreq, shelfType, ripple);

    return true;
  }

  /**
   * a simple normalised fir low pass filter
   * @params order	number of delay coefficients
   */
  bool setSimpleLpf(int order) {
    filterOrder = order;
    clearMemory();
    allocateBufferMemory();
    firCoefficients = new float[filterOrder];
    iirCoefficients = new float[filterOrder];
    std::fill(iirCoefficients, iirCoefficients + filterOrder, 0);
    int coef = 1;
    float gain = 0;
    for (int j = 0; j < filterOrder; j++) {
      if (j == 0) {
        coef = 1;
      } else {
        coef = coef * (filterOrder - j) / j;
      }

      firCoefficients[j] = (float)coef;
      gain += firCoefficients[j];
    }

    // was `<=`: an off-by-one that wrote one element past the
    // filterOrder-sized array (pre-existing, unrelated to the float type
    // change on this line)
    for (int j = 0; j < filterOrder; j++) {
      firCoefficients[j] /= gain;
    }

    return true;
  }

protected:
  /** increment the buffer index and wrap it to the filter order */
  void incBufferIndex() {
    bufferIndex++;
    bufferIndex %= filterOrder;
  }

  /**
   * checks internal memory storage of filter coeffcients and deletes if
   * required
   **/
  void clearMemory() {
    if (firCoefficients) {
      delete[] firCoefficients;
    }

    if (iirCoefficients) {
      delete[] iirCoefficients;
    }
  }

  /**
   * will allocate memory to a buffer given the current filter order and set all
   * values == 0.00
   */
  void allocateBufferMemory() {
    if (firBuffer) {
      delete[] firBuffer;
    }

    if (iirBuffer) {
      delete[] iirBuffer;
    }
    firBuffer = new float[filterOrder];
    iirBuffer = new float[filterOrder];
    std::fill(firBuffer, firBuffer + filterOrder, 0);
    std::fill(iirBuffer, iirBuffer + filterOrder, 0);

    if (firCoefficients) {
      delete[] firCoefficients;
    }

    if (iirCoefficients) {
      delete[] iirCoefficients;
    }

    if (firTemp) {
      delete[] firTemp;
    }

    if (iirTemp) {
      delete[] iirTemp;
    }

    firCoefficients = new float[22];
    iirCoefficients = new float[22];
    firTemp = new float[22];
    iirTemp = new float[22];
    std::fill(firCoefficients, firCoefficients + 22, 0);
    std::fill(iirCoefficients, iirCoefficients + 22, 0);
    std::fill(firTemp, firTemp + 22, 0);
    std::fill(iirTemp, iirTemp + 22, 0);
  }

  /** root mean square of signal over a specific sample window */
  effectsuite_t rms(effectsuite_t sample) {
    rmsBuffer[rmsBufferIndex] = sample;
    effectsuite_t rmsValue = 0;
    for (int j = 0; j < rmsBufferIndex; j++) {
      int i = ((rmsBufferIndex - j) + rmsWindowSize) % rmsWindowSize;
      rmsValue += rmsBuffer[i] * rmsBuffer[i];
    }

    //	printf("samp: %e\tsum: %e\n", sample, rmsValue);
    rmsValue /= rmsWindowSize;
    rmsValue = sqrt(rmsValue);

    rmsBufferIndex++;
    rmsBufferIndex %= rmsWindowSize;

    return rmsValue;
  }

protected:
  // These stay native float regardless of PREFER_FIXEDPOINT, unlike the
  // rest of this file: applyFilter()'s IIR recursion feeds firBuffer/
  // iirBuffer back into itself every sample, and soft_float_t's ~15-bit
  // mantissa (vs float's ~24-bit) lets that feedback state slowly diverge
  // from a real-FPU reference -- measured ~11% peak drift, growing over
  // the run, for a Chebyshev filter's feedback state. Filter *design*
  // (setChebyICoefficients(), setSimpleLpf()) only runs at
  // setup/reconfiguration time, never per-sample, so there's no per-sample
  // FPU cost being traded away by keeping these float either.

  /** Numerator coefficients in delay filter
          firCoefficients[0] z^0 coeffcieint
          firCoefficients[1] z^-1 coefficient
   */
  float *firCoefficients = 0;
  /** Denomiator coefficients in delay filter
          @see firCoefficients
   */
  float *iirCoefficients = 0;
  /** hold temporary values for fir coeffcient buffer*/
  float *firTemp = 0;
  /** hold temporary values for iir coeffcient buffer*/
  float *iirTemp = 0;
  /** buffer to hold forward delay sample data
   */
  float *firBuffer = 0;
  /** buffer to hold backward delay sample data
   */
  float *iirBuffer = 0;
  /**current buffer index*/
  int bufferIndex = 0;
  /** order of delay filter including the zero delay coefficients*/
  int filterOrder = 0;
  /***/
  int samplingRate = 0;
  /** window size in samples of rms window*/
  const int rmsWindowSize = 128;
  /** current write index of rmsBuffer */
  int rmsBufferIndex = 0;
  /** RMS window buffer */
  effectsuite_t *rmsBuffer = new effectsuite_t[rmsWindowSize];

};

/**
 * @brief SimpleLPF
 * @author Matthew Hamilton
 * @ingroup effects
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class SimpleLPF : public FilterEffectBase<effectsuite_t> {
public:
  /**
   * Constructor: Intialised with the order of FIR filter
   * minimum cutoff frequency is 1/sampleRate
   * @param cutoff	normalised cutoff frequency [0, 1);
   * @param order	filter order
   */
  SimpleLPF(effectsuite_t cutoff, int order) {
    this->changeChebyICoefficients(cutoff, false, .1, order);
  };

  SimpleLPF(SimpleLPF &copy) = default;

  /** destructor*/
  ~SimpleLPF() = default;

  SimpleLPF* clone(){
    return new SimpleLPF(*this);
  }

};

/**
 * @brief Simple Chorus effect with a single delay voice and mono output Chorus is
 * effective between 15 and 20 miliseconds delay of original audio. Requires the
 * sample rate when initialising.
 * @ingroup effects
 * @author Matthew Hamilton
 * @copyright MIT License
 **/
template <typename effectsuite_t = effectsuite_t_default>
class SimpleChorus : public DelayEffectBase<effectsuite_t>,
                     public ModulationBaseClass<effectsuite_t>,
                     public SimpleLPF<effectsuite_t>{
public:
  /**
   * Constructor: initialises the effect parameters
   * Since the class inherits DelayEffectBase it must set a
   * delay buffer size when initialisi.
   */
//  SimpleChorus() : SimpleLPF(0.0001, 4) {}
  SimpleChorus(int extSampleRate=44100) :
        DelayEffectBase<effectsuite_t>(static_cast<int>(0.031 * extSampleRate)),
        ModulationBaseClass<effectsuite_t>(extSampleRate),
        SimpleLPF<effectsuite_t>(0.0001, 4) {
    swing = 0.005 * this->sampleRate;
    base = 0.015 * this->sampleRate;
    if (this->sampleRate != 0)
      setRandLfo();
  }

  SimpleChorus(SimpleChorus &copy)=default;

  ~SimpleChorus() = default;

  /**
   * apply chorus effect to audio sample
   * @param inputSample input audio sample
   * @return processed audio sample
   */
  virtual effectsuite_t processFloat(effectsuite_t inputSample) override {
    this->delaySample(inputSample);
    const effectsuite_t waveDelay = getModSignal();
    const effectsuite_t delayAmount =
        ((int(this->currentDelayWriteIndex - waveDelay) + this->delayTimeSamples) %
         this->delayTimeSamples) +
        ((this->currentDelayWriteIndex - waveDelay) -
         trunc(this->currentDelayWriteIndex - waveDelay));
    const effectsuite_t out = .0 * inputSample + 1. * this->getInterpolatedOut(delayAmount);
    return out;
  }

  /**
   * set parameters and internal sample rate
   * @param extSampleRate external sample rate
   */
  void setupChorus(effectsuite_t extSampleRate) {
    this->setupModulationBaseClass(extSampleRate);
    this->setupDelayEffectBase(effectsuite_t(extSampleRate) * .1);
    //    SimpleLPF(0.0004,4)
    this->setChebyICoefficients(0.00005, false, 0);

    swing = readSpeed * extSampleRate * 5;
    base = readSpeed * extSampleRate * 20;
    setRandLfo();
  }

  /**
   * set the 'swing' of the chorus: The amount intensity of vibrato in the delay
   * signal
   * @param swingAmount <#swingAmount description#>
   */
  void setSwing(effectsuite_t swingAmount) { swing = swingAmount * this->sampleRate; }

  /**
   * sets the 'base' of the chorus: the minimum delay in the signal.
   * @param baseAmount <#baseAmount description#>
   */
  void setBase(effectsuite_t baseAmount) { base = baseAmount * this->sampleRate; }

  SimpleChorus* clone() override {
    return new SimpleChorus(*this);
  }

protected:
  /** swing range of delayed audio index in samples
   typical maximum swing would be 15 milliseconds
          (i.e. 0.015*sampleRate)*/
  effectsuite_t swing;
  /** minimum delay in samples. Typically 10 milliseconds */
  effectsuite_t base;
  /** the minimum value of the lowpassed random modulation signal*/
  effectsuite_t modMin = .5;
  /** the maximum value of the lowpassed random modulation signal*/
  effectsuite_t modMax = .5;
  /** the normalising factor of the random delay signal*/
  effectsuite_t modNorm = 1 / (modMax - modMin);
  const effectsuite_t readSpeed = ((this->readNoise() + 1) * .5) * .0005;

  /**
   * modulation signal scaling equation: ((n - modMin)*modNorm*swing) + base
   * modulates a random white noise signal by lowpass filtering then
   * scaling to a range between 15 to 25 miliseconds of delay.
   **/
  effectsuite_t getModSignal() { return (this->readTable(readSpeed) * swing) + base; }

  void setRandLfo() {
    std::fill(this->iirBuffer, this->iirBuffer + this->filterOrder, .5);
    for (int i = 0; i < this->sampleRate; i++) {
      this->waveTable[i] = (this->readNoise() + 1) * .5;
      //        waveTable[i] = applyFilter((readNoise()+1)*.5);
      if (this->waveTable[i] < modMin)
        modMin = this->waveTable[i];
      if (this->waveTable[i] > modMax) {
        modMax = this->waveTable[i];
      }
    }

    modNorm = 1 / (modMax - modMin);

    // normalises the delay signal
    for (int i = 0; i < this->sampleRate; i++) {
      this->waveTable[i] -= modMin;
      this->waveTable[i] *= modNorm;
    }

    //  setOffSine();

    // this code fades out at the end and fades in at the start
    // to avoid any discontinuities int the signal.
    //    const int fadeSize = 10000;
    //    const effectsuite_t fadeSpeed = 2*M_PI/fadeSize;
    //    for (int i = 0; i < fadeSize; i++)
    //    {
    //        const int fadeIndex = ((sampleRate-fadeSize/2)+i)%sampleRate;
    //        waveTable[fadeIndex] *= (1+cos(fadeSpeed*i))*.5;
    //    }
  }
};

/**
 * @brief Delay effect that filters the repeat delay 
 * @ingroup effects
 * @author Matthew Hamilton
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class FilteredDelay : public DelayEffectBase<effectsuite_t>, public FilterEffectBase<effectsuite_t> {
public:
  /** Constructor */
  FilteredDelay(int delayInSamples, int sample_rate=44100) : DelayEffectBase<effectsuite_t>(sample_rate) {
    this->delayTimeSamples = delayInSamples;
    this->changeChebyICoefficients(.05, true, .1, 4);
  };

  FilteredDelay(FilteredDelay&copy) = default;

  /** Destructor */
  ~FilteredDelay() = default;

  /** setDelayGain: sets the delay gain to a value between 1 and -1
   * @param gainrequired delay gain. Values beyond 1 and -1
   * are capped to the maximum to avoid idiocy.
   * Negative velus invoke a phase inversion.
   * @see setFeedbackGain
   */
  void setDelayGain(effectsuite_t gain) {
    capGain(gain);
    delayGain = gain;
  }

  /**
   * setDelayGain: sets the feedback gain to a value between 1 and -1
   * @param gain		required delay gain. Values beyond 1 and -1
   * are capped to the maximum to avoid idiocy.
   * Negative velus invoke a phase inversion.
   * @see setDelayGain
   */
  void setFeedbackGain(effectsuite_t gain) {
    capGain(gain);
    feedbackGain = gain;
  }

  /**apply the DSP effect*/
  effectsuite_t processFloat(effectsuite_t inputSample) override {
    this->delaySample(
        this->applyFilter((inputSample * delayGain) +
                    feedbackGain * this->getInterpolatedOut(this->currentDelayWriteIndex)));
    const effectsuite_t out = this->getInterpolatedOut(this->currentDelayWriteIndex) + inputSample;
    return out;
  }

  effect_t process(effect_t inputSample) override {
    return this->active_flag ? (effect_t)(32767.0 * processFloat(static_cast<effectsuite_t>(inputSample)/32767.0)) : inputSample;
  }

  FilteredDelay *clone() override {
    return new FilteredDelay(*this);
  }

protected:
  /**
   * capGain: caps gain to a range of 1 and -1;
   * @param gain address of gain value
   */
  void capGain(effectsuite_t &gain) {
    if (gain > 1.) {
      gain = 1.;
    } else if (gain < -1.) {
      gain = -1.;
    }
    return;
  }

protected:
  effectsuite_t delayGain = .707, feedbackGain = 0.0;
};

/**
 * @brief Simple Delay effect consiting of a single tap delay with Effect Gain and
 * feed back controls
 * Constructor requires internal delay in samples
 * @ingroup effects
 * @see process
 * @author Matthew Hamilton
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class SimpleDelay : public DelayEffectBase<effectsuite_t>, public EffectSuiteBase<effectsuite_t> {
public:
  /**
   * Constructor: DigitalEffect Base Must Be initialised
   * @param delayInSamples Set the amount of delay in samples
   * @see DelayEffectBase constructor
   */
  // was missing the DelayEffectBase(maxDelayInSamples) base-constructor
  // call entirely -- delayBuffer stayed null and processFloat() below
  // dereferenced it immediately (pre-existing bug, fixed here since this
  // constructor needed touching for the template conversion anyway)
  SimpleDelay(int maxDelayInSamples=8810, int samplingRate=44100)
      : DelayEffectBase<effectsuite_t>(maxDelayInSamples) {
    writeHeadIndex = 0;
    readHeadIndex = 1;
    currentDelaySamples = maxDelayInSamples;
    targetDelaySamples = maxDelayInSamples;
    setDelayTransitionTime(0.5);
  }

  SimpleDelay(SimpleDelay &copy) = default;

  /** Destructor. */
  ~SimpleDelay() = default;

  /**
   * setDelayGain: sets the delay gain to a value between 1 and -1
   * @param gain		required delay gain. Values beyond 1 and -1
   * are capped to the maximum to avoid idiocy.
   * Negative velus invoke a phase inversion.
   * @see setFeedbackGain
   */
  void setDelayGain(effectsuite_t gain) {
    capGain(gain);
    delayGain = gain;
  }

  /**
   * setDelayGain: sets the feedback gain to a value between 1 and -1
   * @param gain		required delay gain. Values beyond 1 and -1
   * are capped to the maximum to avoid idiocy.
   * Negative velus invoke a phase inversion.
   * @see setDelayGain
   */
  void setFeedbackGain(effectsuite_t gain) {
    capGain(gain);
    feedbackGain = gain;
  }

  /**
   * Apply delay and return input sample along with delay buffer signal
   * @param inputSample <#inputSample description#>
   * @return <#return value description#>
   */
  effectsuite_t processFloat(effectsuite_t inputSample) override {
    // write sample
    this->delayBuffer[writeHeadIndex] = inputSample;
    writeHeadIndex++;
    writeHeadIndex %= this->maxDelayBufferSize;

    // read sample
    effectsuite_t outSample = getSplineOut(readHeadIndex) + (inputSample * 1);
    if (delayTimeChanged) {
      count++;
      const effectsuite_t difference = (currentDelaySamples - targetDelaySamples);
      const effectsuite_t increment = delayIncrement * (difference / fabs(difference));
      currentDelaySamples -= increment;
      readHeadIndex += 1 + increment;
      readHeadIndex = std::fmod(readHeadIndex, this->maxDelayBufferSize);
      if (count > floor(delayTransitionTimeInSamples)) {
        currentDelaySamples = targetDelaySamples;
        readHeadIndex = floor(readHeadIndex);
        delayTimeChanged = false;
      }
    } else {
      // soft_float_t doesn't implement operator++ (no other call site in
      // the codebase needs it, so the class doesn't carry the extra API)
      readHeadIndex = readHeadIndex + 1;
      readHeadIndex = std::fmod(readHeadIndex, this->maxDelayBufferSize);
    }
    return outSample;
  }

  effect_t process(effect_t inputSample) override {
    return this->active_flag ? (effect_t)(32767.0 * processFloat(static_cast<effectsuite_t>(inputSample)/32767.0)) : inputSample;
  }

  /**
   <#Description#>
   @param delayInSamples <#delayInSamples description#>
   */
  void setupSimpleDelay(int delayInSamples) {
    this->setupDelayEffectBase(delayInSamples);
  }
  /**
   <#Description#>
   @param delayInSample <#delayInSample description#>
   */
  void setDelayTime(effectsuite_t delayInSamples) {
    delayTimeChanged = true;
    targetDelaySamples = delayInSamples;
    const effectsuite_t delayTimeDifference = currentDelaySamples - targetDelaySamples;
    delayIncrement = delayTimeDifference / delayTransitionTimeInSamples;
    count = 0;
  }
  /**
   <#Description#>
   @param seconds <#seconds description#>
   */
  void setDelayTransitionTime(effectsuite_t seconds) {
    delayTransitionTime = seconds;
    delayTransitionTimeInSamples = seconds * sampleRate;
  }

  SimpleDelay* clone() override {
    return new SimpleDelay(*this);
  }

protected:
  /**
   * capGain: caps gain to a range of 1 and -1;
   * @param gain address of gain value
   */
  void capGain(effectsuite_t &gain) {
    if (gain > 1.) {
      gain = 1.;
    } else if (gain < -1.) {
      gain = -1.;
    }
    return;
  }
  /**
   * get a cubic spline interpolated out from the wave table
   * Derived from code by Alec Wright at repository:
   * https://github.com/Alec-Wright/Chorus
   * @authors Matthew Hamilton, Alec Wright
   * @param bufferIndex the required buffer index
   * @return returns interpolated value as effectsuite_t
   */
  effectsuite_t getSplineOut(effectsuite_t bufferIndex) {
    const int n0 = floor(bufferIndex);
    const int n1 = (n0 + 1) % this->maxDelayBufferSize;
    const int n2 = (n0 + 2) % this->maxDelayBufferSize;
    const effectsuite_t alpha = bufferIndex - n0;

    const effectsuite_t a = this->delayBuffer[n1];
    const effectsuite_t c = ((3 * (this->delayBuffer[n2] - this->delayBuffer[n1])) -
                      (3 * (this->delayBuffer[n1] - this->delayBuffer[n0]))) *
                     0.25;
    const effectsuite_t b = (this->delayBuffer[n2] - this->delayBuffer[n1]) - (2 * c * 0.33333);
    const effectsuite_t d = (-c) * 0.33333;
    return a + (b * alpha) + (c * alpha * alpha) + (d * alpha * alpha * alpha);
  }

protected: // member vairables
  effectsuite_t delayGain = .707;
  effectsuite_t feedbackGain = 0.;
  effectsuite_t readHeadIndex;
  unsigned int writeHeadIndex;
  effectsuite_t currentDelaySamples;
  effectsuite_t targetDelaySamples;
  /** increment when transition from current to target delay per sample set by
   * delayTransitionTime*/
  effectsuite_t delayIncrement;
  /** inverse of delay increment: for working out whole number cyles to reach
   * target delay*/
  effectsuite_t invDelayIncrement;
  /** time in seconds to transition from one delay to another*/
  effectsuite_t delayTransitionTime;
  effectsuite_t delayTransitionTimeInSamples;
  int sampleRate;
  int count = 0;
  bool delayTimeChanged = false;
};

/**
 * @brief Simple Flanger Effect Consistig of a single voice flanger
 * The flanger has an effective range between 0 and 15 miliseconds
 * in this case dleay buffer should be set to sampleRate*3/200
 * Constructor requires internal delay in samples
 * @ingroup effects
 * @see process
 * @author Matthew Hamilton
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class SimpleFlanger : public DelayEffectBase<effectsuite_t>, public EffectSuiteBase<effectsuite_t> {
public:
  /**
   * Constructor: DigitalEffect Base Must Be initialised
   * @see DelayEffectBase constructor
   */
  SimpleFlanger() = default;
  SimpleFlanger(SimpleFlanger&copy) = default;
  SimpleFlanger(effectsuite_t extSampleRate=44100)
      : DelayEffectBase<effectsuite_t>(static_cast<int>(extSampleRate * 0.02)) {}

  /** Destructor. */
  ~SimpleFlanger() = default;

  /**
   * setEffectGain: sets the effect gain to a value between 1 and -1
   * @param gain		required delay gain. Values beyond 1 and -1
   * are capped to the maximum to avoid idiocy.
   * Negative velus invoke a phase inversion.
   */
  void setEffectGain(effectsuite_t gain) { effectGain = capGain(gain); }

  /**
   * <#Description#>
   * @param depth <#depth description#>
   */
  void setDepth(const effectsuite_t depth) {
    if (depth > effectsuite_t(this->delayTimeSamples))
      modulationDepth = effectsuite_t(this->delayTimeSamples) - 1;
    else
      modulationDepth = depth;
  }

  /**
   * <#Description#>
   *  @param rate <#rate description#>
   */
  void setRate(const effectsuite_t rate) {
    modulationRate = rate;
    setAngleDelta();
  }

  /**
   *  setEffectGain: sets the parameters for effect
   *   @param	gain	effect gain
   *   @param	depth	depth of modulation in samples
   *   @param	rate	rate of modulation in Hz
   */
  void setEffectParams(effectsuite_t gain, effectsuite_t depth, effectsuite_t rate) {
    setEffectGain(gain);
    setDepth(depth);
    setRate(rate);
  }

  /**Apply the DSP effect*/
  effectsuite_t processFloat(effectsuite_t inputSample) override {
    this->delaySample(inputSample);
    const effectsuite_t out = ((1 - fabs(effectGain * .2)) * (inputSample) +
                        (effectGain * this->getInterpolatedOut(modulationIndex)));
    updateModulation();
    return out;
  }

  void setupSimpleFlanger(effectsuite_t extSampleRate) {
    this->setupDelayEffectBase(extSampleRate * .02);
    timeStep = 1. / extSampleRate;
    setEffectParams(.707, extSampleRate * .02, .1);
  }

  SimpleFlanger* clone() override {
    return new SimpleFlanger(*this);
  }

protected:
  /**
   * capGain: caps gain to a range of 1 and -1;
   *  @param gain address of gain value
   */
  effectsuite_t capGain(effectsuite_t gain) {
    if (gain > 1.) {
      gain = 1.;
    } else if (gain < -1.) {
      gain = -1.;
    }
    return gain;
  }

  /**
   *  setAngleDelta: sets the angleDelta for delay modulation
   **/
  void setAngleDelta() {
    const effectsuite_t cyclesPerSample = modulationRate * timeStep;
    angleDelta = cyclesPerSample * 2.0f * PI;
  }

  /**
   *  updateModulation: updates the modulationIndex by the correct increment
   **/
  void updateModulation() {
    modulationAngle += angleDelta;
    modulationIndex = (this->currentDelayWriteIndex -
                       (modulationDepth * (1 + (sin(modulationAngle))))) -
                      12;
    modulationIndex =
        ((int(modulationIndex) + this->delayTimeSamples) % this->delayTimeSamples) +
        (modulationIndex - floor(modulationIndex));
  }

protected:

  effectsuite_t modulationDepth = 1000, modulationRate = 0, effectGain = .01;

  effectsuite_t modulationIndex = 0;

  /** 1/sampleRate: The time in seconds between samples*/
  effectsuite_t timeStep = 1. / 44100.;

  /** 2 * pi * modulationRate / sampleRate */
  effectsuite_t modulationConstant, modulationAngle = 0;

  // const effectsuite_t cyclesPerSample = modulationRate * timeStep;
  /**increment value for modulation signal*/
  effectsuite_t angleDelta = 2.0f * PI * timeStep;
};

/**
 * @brief EnvelopeFilter
 * @author Matthew Hamilton
 * @ingroup effects
 * @copyright MIT License
 */
template <typename effectsuite_t = effectsuite_t_default>
class EnvelopeFilter : public FilterEffectBase<effectsuite_t> {
public:
  /** Constructor */
  EnvelopeFilter() : envelopeFollower(.00006, 4) {
    // NOTE: Initialising chebyshev coeffcients allocates memory, perhaps alter
    // so that memory is already pre allocated
    this->changeChebyICoefficients(.01, false, .1, 4);
    envelopeFollower.setChebyICoefficients(.00006, false, 0);
  };

  /** Desructor */
  ~EnvelopeFilter() = default;
  /**
   * main process method: applies an envelope filter to the incoming signal
   * sample
   * @params sample		incoming signal sample value
   * @returns processed sample value
   */
  effectsuite_t processFloat(effectsuite_t sample) {
    this->setChebyICoefficients(0.001 + envelopeFollower.envelope(2 * sample), false,
                          .1); // Offset avoids zero cutoff value
    return this->applyFilter(sample);
  }

  EnvelopeFilter* clone() override {
    return new EnvelopeFilter(*this);
  }

protected:
  /**
   * this follows the signal envelope and alters the internallow pass filter
   * cutoff
   */
  SimpleLPF<effectsuite_t> envelopeFollower;
};

} // namespace effectsuite_tools

