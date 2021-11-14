#pragma once

#include "AudioConfig.h"
#ifdef USE_STK
#include "AudioEffects/AudioEffect.h"
#include "Effect.h"
#include "Stk.h"
#include "Chorus.h"
#include "Echo.h"
#include "FreeVerb.h"
#include "JCRev.h"
#include "NRev.h"
#include "PRCRev.h"
#include "LentPitShift.h"
#include "PitShift.h"


namespace audio_tools {

/**
 * Use any effect from the STK framework: e.g. Chorus, Echo, FreeVerb, JCRev,
 * PitShift... https://github.com/pschatzmann/Arduino-STK
 *
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
 * Chorus Effect
 */
class STKChorus : public AudioEffect, public stk::Chorus {
public:
  STKChorus(float baseDelay = 6000) : stk::Chorus(baseDelay) {}

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * Echo Effect
 */
class STKEcho : public AudioEffect, public stk::Echo {
public:
  STKEcho(unsigned long maximumDelay = (unsigned long)Stk::sampleRate())
      : stk::Echo(maximumDelay) {}

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * Jezar at Dreampoint's FreeVerb, implemented in STK.
 */
class STKFreeVerb : public AudioEffect, public stk::FreeVerb {
public:
  STKFreeVerb() = default;
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * John Chowning's reverberator class.
 */
class STKChowningReverb : public AudioEffect, public stk::JCRev {
public:
  STKChowningReverb() = default;

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * CCRMA's NRev reverberator class.
 */
class STKNReverb : public AudioEffect, public stk::NRev {
public:
  STKNReverb(float t60 = 1.0) : NRev(t60) {}
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * Perry's simple reverberator class
 */
class STKPerryReverb : public AudioEffect, public stk::PRCRev {
public:
  STKPerryReverb(float t60 = 1.0) : PRCRev(t60) {}
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * Pitch shifter effect class based on the Lent algorithm
 */
class STKLentPitShift : public AudioEffect, public stk::LentPitShift {
public:
  STKLentPitShift(float periodRatio = 1.0, int tMax = 512)
      : stk::LentPitShift(periodRatio, tMax) {}

  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

/**
 * Pitch shifter effect class based on the Lent algorithm
 */
class STKPitShift : public AudioEffect, public stk::PitShift {
public:
  STKPitShift() = default;
  virtual effect_t process(effect_t in) {
    // just convert between int16 and float
    float value = static_cast<float>(in) / 32767.0;
    return tick(value) * 32767.0;
  }
};

} // namespace audio_tools

#endif