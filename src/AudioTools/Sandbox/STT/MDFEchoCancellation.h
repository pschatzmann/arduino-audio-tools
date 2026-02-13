/* Copyright (C) 2003-2008 Jean-Marc Valin
 * Copyright (C) 2024 Phil Schatzmann (Header-only adaptation)
 *
 * Echo canceller based on the MDF algorithm
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "MDFEchoCancellationConfig.h"
#include "AudioTools/AudioLibs/AudioFFT.h"

// Control requests
#define ECHO_GET_FRAME_SIZE 3
#define ECHO_SET_SAMPLING_RATE 24
#define ECHO_GET_SAMPLING_RATE 25
#define ECHO_GET_IMPULSE_RESPONSE_SIZE 27
#define ECHO_GET_IMPULSE_RESPONSE 29

// Helper macros for floating point mode
#ifndef FIXED_POINT
#define PSEUDOFLOAT(x) (x)
#define FLOAT_MULT(a, b) ((a) * (b))
#define FLOAT_AMULT(a, b) ((a) * (b))
#define FLOAT_MUL32(a, b) ((a) * (b))
#define FLOAT_DIV32(a, b) ((a) / (b))
#define FLOAT_EXTRACT16(a) (a)
#define FLOAT_EXTRACT32(a) (a)
#define FLOAT_ADD(a, b) ((a) + (b))
#define FLOAT_SUB(a, b) ((a) - (b))
#define FLOAT_DIV32_FLOAT(a, b) ((a) / (b))
#define FLOAT_MUL32U(a, b) ((a) * (b))
#define FLOAT_SHL(a, b) (a)
#define FLOAT_LT(a, b) ((a) < (b))
#define FLOAT_GT(a, b) ((a) > (b))
#define FLOAT_DIVU(a, b) ((a) / (b))
#define FLOAT_SQRT(a) (sqrtf(a))

// Math operation macros for floating point
#define ABS(x) (((x) < 0) ? (-(x)) : (x))
#define ABS16(x) (((x) < 0) ? (-(x)) : (x))
#define ABS32(x) (((x) < 0) ? (-(x)) : (x))
#define MIN16(a, b) ((a) < (b) ? (a) : (b))
#define MAX16(a, b) ((a) > (b) ? (a) : (b))
#define MIN32(a, b) ((a) < (b) ? (a) : (b))
#define MAX32(a, b) ((a) > (b) ? (a) : (b))

#define QCONST16(x, bits) (x)
#define QCONST32(x, bits) (x)
#define NEG16(x) (-(x))
#define NEG32(x) (-(x))
#define EXTRACT16(x) (x)
#define EXTEND32(x) (x)
#define SHR16(a, shift) (a)
#define SHL16(a, shift) (a)
#define SHR32(a, shift) (a)
#define SHL32(a, shift) (a)
#define PSHR16(a, shift) (a)
#define PSHR32(a, shift) (a)
#define VSHR32(a, shift) (a)
#define SATURATE16(x, a) (x)
#define SATURATE32(x, a) (x)
#define ADD16(a, b) ((a) + (b))
#define SUB16(a, b) ((a) - (b))
#define ADD32(a, b) ((a) + (b))
#define SUB32(a, b) ((a) - (b))
#define MULT16_16(a, b) ((echo_word32_t)(a) * (echo_word32_t)(b))
#define MAC16_16(c, a, b) ((c) + (echo_word32_t)(a) * (echo_word32_t)(b))
#define MULT16_32_Q15(a, b) ((a) * (b))
#define MULT16_16_P15(a, b) ((a) * (b))
#define MULT16_32_P15(a, b) ((a) * (b))
#define MULT16_16_Q15(a, b) ((a) * (b))
#define DIV32_16(a, b) (((echo_word32_t)(a)) / (echo_word16_t)(b))
#define DIV32(a, b) (((echo_word32_t)(a)) / (echo_word32_t)(b))
#define WORD2INT(x) \
  ((x) < -32767.5f  \
       ? -32768     \
       : ((x) > 32766.5f ? 32767 : (echo_int16_t)floorf(.5f + (x))))
#define MULT16_16_Q14(a, b) ((a) * (b))
#define MULT16_16_Q13(a, b) ((a) * (b))
#define MULT16_16_P13(a, b) ((a) * (b))
#define MULT16_16_P14(a, b) ((a) * (b))
#define MULT16_32_Q14(a, b) ((a) * (b))
#define MAC16_32_Q15(c, a, b) ((c) + (a) * (b))

#define TOP16(x) (x)
#define WEIGHT_SHIFT 0

#else
// Fixed-point mode macros
#define PSEUDOFLOAT(x) (echo_float_from_double(x))
#define FLOAT_MULT(a, b) (echo_float_mult(a, b))
#define FLOAT_MUL32U(a, b) (echo_float_mul32_u(a, b))
#define FLOAT_ADD(a, b) (echo_float_add(a, b))
#define FLOAT_SUB(a, b) (echo_float_sub(a, b))
#define FLOAT_LT(a, b) (echo_float_lt(a, b))
#define FLOAT_GT(a, b) (echo_float_gt(a, b))
#define FLOAT_DIVU(a, b) (echo_float_divu(a, b))
#define FLOAT_SQRT(a) (echo_float_sqrt(a))
#define FLOAT_EXTRACT16(a) (echo_float_extract16(a))
#define FLOAT_SHL(a, b) (echo_float_shl(a, b))

// Math operation macros (same for fixed-point)
#define ABS(x) (((x) < 0) ? (-(x)) : (x))
#define ABS16(x) (((x) < 0) ? (-(x)) : (x))
#define ABS32(x) (((x) < 0) ? (-(x)) : (x))
#define MIN16(a, b) ((a) < (b) ? (a) : (b))
#define MAX16(a, b) ((a) > (b) ? (a) : (b))
#define MIN32(a, b) ((a) < (b) ? (a) : (b))
#define MAX32(a, b) ((a) > (b) ? (a) : (b))

#define WORD2INT(x) \
  ((x) < -32767.5f  \
       ? -32768     \
       : ((x) > 32766.5f ? 32767 : (echo_int16_t)floorf(.5f + (x))))

#define TOP16(x) (x)
#define WEIGHT_SHIFT 0

#endif  // !FIXED_POINT

// Forward declaration for FFT interface
namespace audio_tools {

// Type definitions (must be before echo_float_t struct)
typedef int16_t echo_int16_t;
typedef uint16_t echo_uint16_t;
typedef int32_t echo_int32_t;
typedef uint32_t echo_uint32_t;

typedef echo_int16_t echo_word16_t;
typedef echo_int32_t echo_word32_t;
typedef echo_word32_t echo_mem_t;

#ifdef FIXED_POINT
// Forward declarations for operators
struct echo_float_t;
inline echo_float_t echo_float_mult(echo_float_t a, echo_float_t b);
inline echo_float_t echo_float_mul32_u(float a, float b);

// Fixed-point type for pseudo-float operations
typedef struct echo_float_t {
  int16_t m;  // Mantissa
  int16_t e;  // Exponent
  
  // Constructor for initialization
  constexpr echo_float_t(int16_t mantissa = 0, int16_t exponent = 0) 
    : m(mantissa), e(exponent) {}
  
  // Operator overloads for arithmetic
  inline echo_float_t operator*(const echo_float_t& other) const {
    return echo_float_mult(*this, other);
  }
  
  inline echo_word32_t operator*(int16_t scalar) const {
    // Result is a regular integer when multiplying float by int
    int32_t result = ((int32_t)m * scalar);
    if (e >= 14) {
      return result << (e - 14);
    } else {
      return result >> (14 - e);
    }
  }
  
  inline echo_word32_t operator*(int32_t scalar) const {
    // Result is a regular integer when multiplying float by int
    int64_t result = ((int64_t)m * scalar);
    if (e >= 14) {
      return result << (e - 14);
    } else {
      return result >> (14 - e);
    }
  }
  
  friend inline echo_word32_t operator*(int32_t scalar, const echo_float_t& f) {
    return f * scalar;
  }
} echo_float_t;

// Fixed-point arithmetic helper functions
inline echo_float_t echo_float_from_double(double x) {
  echo_float_t r;
  if (x == 0) {
    r.m = 0;
    r.e = 0;
    return r;
  }
  int e = 0;
  while (x >= 32768) { x *= 0.5; e++; }
  while (x < 16384) { x *= 2.0; e--; }
  r.m = (int16_t)x;
  r.e = (int16_t)e;
  return r;
}

inline echo_float_t echo_float_mult(echo_float_t a, echo_float_t b) {
  echo_float_t r;
  r.m = (int16_t)((((int32_t)a.m) * b.m) >> 15);
  r.e = a.e + b.e + 15;
  if (r.m > 0) {
    while (r.m < 16384) { r.m <<= 1; r.e--; }
  }
  return r;
}

inline echo_float_t echo_float_mul32_u(float a, float b) {
  return echo_float_from_double(a * b);
}

inline echo_float_t echo_float_add(echo_float_t a, echo_float_t b) {
  if (a.e > b.e) {
    int shift = a.e - b.e;
    if (shift > 15) return a;
    echo_float_t r = {(int16_t)(a.m + (b.m >> shift)), a.e};
    return r;
  } else {
    int shift = b.e - a.e;
    if (shift > 15) return b;
    echo_float_t r = {(int16_t)((a.m >> shift) + b.m), b.e};
    return r;
  }
}

inline echo_float_t echo_float_sub(echo_float_t a, echo_float_t b) {
  if (a.e > b.e) {
    int shift = a.e - b.e;
    if (shift > 15) return a;
    echo_float_t r = {(int16_t)(a.m - (b.m >> shift)), a.e};
    return r;
  } else {
    int shift = b.e - a.e;
    if (shift > 15) {
      echo_float_t r = {(int16_t)(-b.m), b.e};
      return r;
    }
    echo_float_t r = {(int16_t)((a.m >> shift) - b.m), b.e};
    return r;
  }
}

inline bool echo_float_lt(echo_float_t a, echo_float_t b) {
  if (a.e != b.e) return a.e < b.e;
  return a.m < b.m;
}

inline bool echo_float_gt(echo_float_t a, echo_float_t b) {
  if (a.e != b.e) return a.e > b.e;
  return a.m > b.m;
}

inline echo_float_t echo_float_divu(echo_float_t a, echo_float_t b) {
  echo_float_t r;
  if (b.m == 0) {
    r.m = 32767;
    r.e = 15;
    return r;
  }
  r.m = (int16_t)((((int32_t)a.m) << 15) / b.m);
  r.e = a.e - b.e - 15;
  if (r.m > 0) {
    while (r.m < 16384) { r.m <<= 1; r.e--; }
  }
  return r;
}

inline echo_float_t echo_float_sqrt(echo_float_t x) {
  double val = x.m * pow(2.0, x.e - 14);
  return echo_float_from_double(sqrt(val));
}

inline int16_t echo_float_extract16(echo_float_t x) {
  if (x.e > 0) {
    return x.m << (x.e > 15 ? 15 : x.e);
  } else {
    return x.m >> (-x.e > 15 ? 15 : -x.e);
  }
}

inline echo_float_t echo_float_shl(echo_float_t x, int bits) {
  echo_float_t r = {x.m, (int16_t)(x.e + bits)};
  return r;
}


/** Minimum leak estimate for the adaptive filter (0.005 in Q15: 164) */
static const echo_float_t MIN_LEAK = {164, -15};

/** Smoothing coefficient for first variance estimator (0.36 in Q15: 11796) */
static const echo_float_t VAR1_SMOOTH = {11796, -15};

/** Smoothing coefficient for second variance estimator (0.7225 in Q15: 23674) */
static const echo_float_t VAR2_SMOOTH = {23674, -15};

/** Update threshold for first variance estimator (0.5 in Q15: 16384) */
static const echo_float_t VAR1_UPDATE = {16384, -15};

/** Update threshold for second variance estimator (0.25 in Q15: 8192) */
static const echo_float_t VAR2_UPDATE = {8192, -15};

/** Backtrack threshold for filter reset (4.0 in Q15: 16384, exp 2) */
static const echo_float_t VAR_BACKTRACK = {16384, 2};

#else
// Floating-point mode

/** Minimum leak estimate for the adaptive filter (0.005) */
static const float MIN_LEAK = 0.005f;

/** Smoothing coefficient for first variance estimator (0.36) */
static const float VAR1_SMOOTH = 0.36f;

/** Smoothing coefficient for second variance estimator (0.7225) */
static const float VAR2_SMOOTH = 0.7225f;

/** Update threshold for first variance estimator (0.5) */
static const float VAR1_UPDATE = 0.5f;

/** Update threshold for second variance estimator (0.25) */
static const float VAR2_UPDATE = 0.25f;

/** Backtrack threshold for filter reset (4.0) */
static const float VAR_BACKTRACK = 4.0f;

#endif  // FIXED_POINT


/** @defgroup EchoState EchoState: Acoustic echo canceller
 *  This is the acoustic echo canceller module.
 *  @{
 */

// Floating point type definition (for non-FIXED_POINT mode)
#ifndef FIXED_POINT
typedef float echo_float_t;
#endif

/**
 * @brief Internal echo canceller state structure
 * 
 * Contains all internal state variables and buffers for the MDF (Multi-Delay
 * block Frequency adaptive filter) echo cancellation algorithm. This structure
 * should never be accessed directly by users; use the EchoCanceller class
 * methods instead.
 * 
 * @warning Direct access to this structure is not recommended and may break
 * encapsulation.
 */
struct EchoState_ {
  int frame_size; /**< Number of samples processed each time */
  int window_size;
  int M;
  int cancel_count;
  int adapted;
  int saturated;
  int screwed_up;
  int C; /** Number of input channels (microphones) */
  int K; /** Number of output channels (loudspeakers) */
  echo_int32_t sampling_rate;
  echo_word16_t spec_average;
  echo_word16_t beta0;
  echo_word16_t beta_max;
  echo_word32_t sum_adapt;
  echo_word16_t leak_estimate;

  echo_word16_t* e;     /* scratch */
  echo_word16_t* x;     /* Far-end input buffer (2N) */
  echo_word16_t* X;     /* Far-end buffer (M+1 frames) in frequency domain */
  echo_word16_t* input; /* scratch */
  echo_word16_t* y;     /* scratch */
  echo_word16_t* last_y;
  echo_word16_t* Y; /* scratch */
  echo_word16_t* E;
  echo_word32_t* PHI; /* scratch */
  echo_word32_t* W;   /* (Background) filter weights */
#ifdef TWO_PATH
  echo_word16_t* foreground; /* Foreground filter weights */
  echo_word32_t
      Davg1; /* 1st recursive average of the residual power difference */
  echo_word32_t
      Davg2; /* 2nd recursive average of the residual power difference */
  echo_float_t Dvar1; /* Estimated variance of 1st estimator */
  echo_float_t Dvar2; /* Estimated variance of 2nd estimator */
#endif
  echo_word32_t* power;  /* Power of the far-end signal */
  echo_float_t* power_1; /* Inverse power of far-end */
  echo_word16_t* wtmp;   /* scratch */
  echo_word32_t* Rf;     /* scratch */
  echo_word32_t* Yf;     /* scratch */
  echo_word32_t* Xf;     /* scratch */
  echo_word32_t* Eh;
  echo_word32_t* Yh;
  echo_float_t Pey;
  echo_float_t Pyy;
  echo_word16_t* window;
  echo_word16_t* prop;
  void* fft_table;
  echo_word16_t *memX, *memD, *memE;
  echo_word16_t preemph;
  echo_word16_t notch_radius;
  echo_mem_t* notch_mem;

  echo_int16_t* play_buf;
  int play_buf_pos;
  int play_buf_started;
};

typedef struct EchoState_ EchoState;

/**
 * @brief FFT state management structure with custom allocator support
 * 
 * Manages FFT operations through an abstract AudioFFTBase driver interface and
 * maintains temporary buffers for real and imaginary components. The structure
 * is templated to support custom memory allocators for embedded systems or
 * specialized memory management requirements.
 * 
 * @tparam Allocator Custom allocator type (defaults to std::allocator<uint8_t>)
 */
template <typename Allocator = std::allocator<uint8_t>>
struct fft_state {
  using FloatAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<float>; /**< Rebound allocator for float vectors */
  
  AudioFFTBase* driver; /**< Pointer to FFT driver implementation */
  int N; /**< FFT size */
  std::vector<float, FloatAllocator> temp_real; /**< Temporary buffer for real components */
  std::vector<float, FloatAllocator> temp_img; /**< Temporary buffer for imaginary components */

  /**
   * @brief Construct FFT state with specified size and driver
   * @param size FFT size (number of points)
   * @param drv Pointer to FFT driver implementation
   * @param alloc Allocator instance for memory management
   */
  fft_state(int size, AudioFFTBase* drv, const Allocator& alloc = Allocator()) 
      : N(size), driver(drv), 
        temp_real(FloatAllocator(alloc)), 
        temp_img(FloatAllocator(alloc)) {
    temp_real.resize(size);
    temp_img.resize(size);
  }
};

/**
 * @brief Acoustic echo canceller using MDF algorithm
 * 
 * High-performance echo cancellation implementation based on the Multi-Delay
 * block Frequency adaptive filter (MDF) algorithm. Supports both single and
 * multi-channel configurations with customizable memory allocation.
 * 
 * The echo canceller operates by:
 * - Learning the echo path between loudspeakers and microphones
 * - Adaptively filtering the playback signal to predict the echo
 * - Subtracting the predicted echo from the captured microphone signal
 * 
 * Features:
 * - Header-only C++ implementation
 * - Custom allocator support for embedded systems
 * - Fixed-point and floating-point arithmetic modes
 * - Two-path filter for improved double-talk handling
 * - Multi-channel support (multiple microphones and speakers)
 * 
 * @tparam Allocator Custom allocator type for memory management
 *                   (defaults to std::allocator<uint8_t>)
 * 
 * @note This implementation requires an external FFT driver implementing
 *       the AudioFFTBase interface from AudioFFT.h
 * 
 * Example usage:
 * @code
 * FFTDriverImpl fftDriver;
 * EchoCanceller<> echo(frameSize, filterLength, fftDriver);
 * echo.setSamplingRate(16000);
 * echo.cancellation(micData, speakerData, outputData);
 * @endcode
 */
template <typename Allocator = std::allocator<uint8_t>>
class MDFEchoCancellation : public AudioStream {
 public:
  /** Initialize echo canceller with single channel (mono)
   * @param filterLength Length of echo cancellation filter in samples
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator instance (optional)
   */
  MDFEchoCancellation(int filterLength, AudioFFTBase& fftDriver, const Allocator& alloc = Allocator())
      : fft_driver(&fftDriver), allocator(alloc),
        filter_length(filterLength), nb_mic(1), nb_speakers(1) {}

  /** Initialize echo canceller with multiple channels
   * @param filterLength Length of echo cancellation filter in samples
   * @param nbMic Number of microphone channels
   * @param nbSpeakers Number of speaker channels
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator instance (optional)
   */
  MDFEchoCancellation(int filterLength, int nbMic, int nbSpeakers,
                AudioFFTBase& fftDriver, const Allocator& alloc = Allocator())
      : fft_driver(&fftDriver), allocator(alloc),
        filter_length(filterLength), nb_mic(nbMic), nb_speakers(nbSpeakers) {}

  /** Destructor */
  ~MDFEchoCancellation() {
    if (!state) return;

    if (state->fft_table) echo_fft_destroy<Allocator>(state->fft_table);

    echoFree(state->e);
    echoFree(state->x);
    echoFree(state->input);
    echoFree(state->y);
    echoFree(state->last_y);
    echoFree(state->Yf);
    echoFree(state->Rf);
    echoFree(state->Xf);
    echoFree(state->Yh);
    echoFree(state->Eh);
    echoFree(state->X);
    echoFree(state->Y);
    echoFree(state->E);
    echoFree(state->W);
#ifdef TWO_PATH
    echoFree(state->foreground);
#endif
    echoFree(state->PHI);
    echoFree(state->power);
    echoFree(state->power_1);
    echoFree(state->window);
    echoFree(state->prop);
    echoFree(state->wtmp);
    echoFree(state->memX);
    echoFree(state->memD);
    echoFree(state->memE);
    echoFree(state->notch_mem);
    echoFree(state->play_buf);
    delete state;
    state = nullptr;
  }

  /** 
   * Perform echo cancellation
   * @param rec Recorded signal (with echo)
   * @param play Playback signal (reference)
   * @param out Output signal (echo removed)
   */
  void cancel(const echo_int16_t* rec, const echo_int16_t* play,
                    echo_int16_t* out) {
    ensureInitialized();
    echoCancellationImpl(state, rec, play, out);
  }

  /** Process captured audio with buffered playback
   * @param rec Recorded signal
   * @param out Output signal (echo removed)
   */
  void capture(const echo_int16_t* rec, echo_int16_t* out) {
    ensureInitialized();
    state->play_buf_started = 1;
    if (state->play_buf_pos >= state->frame_size) {
      echoCancellationImpl(state, rec, state->play_buf, out);
      state->play_buf_pos -= state->frame_size;
      std::memmove(state->play_buf,
                   &state->play_buf[state->frame_size],
                   state->play_buf_pos * sizeof(echo_int16_t));
    } else {
      echoWarning("No playback frame available");
      if (state->play_buf_pos != 0) {
        echoWarning("Internal playback buffer corruption");
        state->play_buf_pos = 0;
      }
      for (int i = 0; i < state->frame_size; i++) out[i] = rec[i];
    }
  }

  /** Buffer playback signal for later processing
   * @param play Playback signal to buffer
   */
  void playback(const echo_int16_t* play) {
    ensureInitialized();
    if (!state->play_buf_started) {
      echoWarning("Discarded first playback frame");
      return;
    }
    if (state->play_buf_pos <= PLAYBACK_DELAY * state->frame_size) {
      for (int i = 0; i < state->frame_size; i++)
        state->play_buf[state->play_buf_pos + i] = play[i];
      state->play_buf_pos += state->frame_size;
      if (state->play_buf_pos <= (PLAYBACK_DELAY - 1) * state->frame_size) {
        echoWarning("Auto-filling buffer");
        for (int i = 0; i < state->frame_size; i++)
          state->play_buf[state->play_buf_pos + i] = play[i];
        state->play_buf_pos += state->frame_size;
      }
    } else {
      echoWarning("Had to discard playback frame");
    }
  }

  /** Reset echo canceller state */
  void reset() {
    ensureInitialized();
    int N = state->window_size;
    int M = state->M;
    int C = state->C;
    int K = state->K;

    state->cancel_count = 0;
    state->screwed_up = 0;

    for (int i = 0; i < N * M; i++) state->W[i] = 0;
#ifdef TWO_PATH
    for (int i = 0; i < N * M; i++) state->foreground[i] = 0;
#endif
    for (int i = 0; i < N * (M + 1); i++) state->X[i] = 0;
    for (int i = 0; i <= state->frame_size; i++) {
      state->power[i] = 0;
      state->power_1[i] = FLOAT_ONE;
      state->Eh[i] = 0;
      state->Yh[i] = 0;
    }
    for (int i = 0; i < state->frame_size; i++) state->last_y[i] = 0;
    for (int i = 0; i < N * C; i++) state->E[i] = 0;
    for (int i = 0; i < N * K; i++) state->x[i] = 0;
    for (int i = 0; i < 2 * C; i++) state->notch_mem[i] = 0;
    for (int i = 0; i < C; i++) state->memD[i] = state->memE[i] = 0;
    for (int i = 0; i < K; i++) state->memX[i] = 0;

    state->saturated = 0;
    state->adapted = 0;
    state->sum_adapt = 0;
    state->Pey = state->Pyy = FLOAT_ONE;
#ifdef TWO_PATH
    state->Davg1 = state->Davg2 = 0;
    state->Dvar1 = state->Dvar2 = FLOAT_ZERO;
#endif
    for (int i = 0; i < 3 * state->frame_size; i++) state->play_buf[i] = 0;
    state->play_buf_pos = PLAYBACK_DELAY * state->frame_size;
    state->play_buf_started = 0;
  }

  /** Control/query echo canceller parameters
   * @param request Request type (ECHO_GET_FRAME_SIZE, etc.)
   * @param ptr Pointer to parameter value
   * @return 0 on success, -1 on error
   */
  int control(int request, void* ptr) {
    switch (request) {
      case ECHO_GET_FRAME_SIZE:
        (*(int*)ptr) = state->frame_size;
        break;
      case ECHO_SET_SAMPLING_RATE:
        state->sampling_rate = (*(int*)ptr);
        state->spec_average = state->frame_size / (float)state->sampling_rate;
        state->beta0 = (2.0f * state->frame_size) / state->sampling_rate;
        state->beta_max = (.5f * state->frame_size) / state->sampling_rate;
        if (state->sampling_rate < 12000)
          state->notch_radius = .9f;
        else if (state->sampling_rate < 24000)
          state->notch_radius = .982f;
        else
          state->notch_radius = .992f;
        break;
      case ECHO_GET_SAMPLING_RATE:
        (*(int*)ptr) = state->sampling_rate;
        break;
      case ECHO_GET_IMPULSE_RESPONSE_SIZE:
        *((echo_int32_t*)ptr) = state->M * state->frame_size;
        break;
      case ECHO_GET_IMPULSE_RESPONSE: {
        int M = state->M, N = state->window_size, n = state->frame_size;
        echo_int32_t* filt = (echo_int32_t*)ptr;
        for (int j = 0; j < M; j++) {
          echo_ifft<Allocator>(state->fft_table, &state->W[j * N], state->wtmp);
          for (int i = 0; i < n; i++) filt[j * n + i] = 32767 * state->wtmp[i];
        }
      } break;
      default:
        return -1;
    }
    return 0;
  }

  /** Get frame size */
  int getFrameSize() { return state->frame_size; }

  /** Set sampling rate
   * @param rate Sampling rate in Hz
   */
  void setSamplingRate(int rate) { control(ECHO_SET_SAMPLING_RATE, &rate); }

  /** Get sampling rate */
  int getSamplingRate() { return state->sampling_rate; }

  /** Get impulse response size */
  int getImpulseResponseSize() { return state->M * state->frame_size; }

  /** Get impulse response
   * @param response Buffer to store impulse response (must be pre-allocated)
   */
  void getImpulseResponse(echo_int32_t* response) {
    control(ECHO_GET_IMPULSE_RESPONSE, response);
  }

  /** Set filter length (must be called before first use)
   * @param len Length of echo cancellation filter in samples
   */
  void setFilterLength(int len) {
    if (initialized) {
      echoWarning("Cannot change filter length after initialization");
      return;
    }
    filter_length = len;
  }

  /** Get filter length */
  int getFilterLength() { return filter_length; }

  /** Set number of microphone channels (must be called before first use)
   * @param num Number of microphone channels
   */
  void setMicChannels(int num) {
    if (initialized) {
      echoWarning("Cannot change mic channels after initialization");
      return;
    }
    nb_mic = num;
  }

  /** Get number of microphone channels */
  int getMicChannels() { return nb_mic; }

  /** Set number of speaker channels (must be called before first use)
   * @param num Number of speaker channels
   */
  void setSpeakerChannels(int num) {
    if (initialized) {
      echoWarning("Cannot change speaker channels after initialization");
      return;
    }
    nb_speakers = num;
  }

  /** Get number of speaker channels */
  int getSpeakerChannels() { return nb_speakers; }

  /** Set FFT driver (must be called before first use)
   * @param fftDriver Reference to FFT driver instance
   */
  void setFFTDriver(AudioFFTBase& fftDriver) {
    if (initialized) {
      echoWarning("Cannot change FFT driver after initialization");
      return;
    }
    fft_driver = &fftDriver;
  }

  /** Get underlying state (for advanced usage) */
  EchoState* getState() { return state; }

 protected:
  EchoState* state = nullptr; /**< Pointer to internal echo canceller state */
  AudioFFTBase* fft_driver; /**< Pointer to FFT driver instance */
  Allocator allocator; /**< Allocator instance for memory management */
  int filter_length; /**< Length of echo cancellation filter */
  int nb_mic; /**< Number of microphone channels */
  int nb_speakers; /**< Number of speaker channels */
  bool initialized = false; /**< Lazy initialization flag */

  /**
   * @brief Ensure echo canceller is initialized (lazy initialization)
   */
  void ensureInitialized() {
    if (initialized) return;
    
    int frameSize = fft_driver->config().length;
    state = echoStateInitMc(frameSize, filter_length, nb_mic, nb_speakers);
    if (state && fft_driver) {
      state->fft_table =
          echo_fft_init<Allocator>(state->window_size, fft_driver, allocator);
    }
    initialized = true;
  }

  /**
   * @brief Allocate memory for array of type T using custom allocator
   * @tparam T Type of objects to allocate
   * @param count Number of objects to allocate
   * @return Pointer to allocated and zero-initialized memory
   * @note Memory is zero-initialized to match calloc behavior
   */
  template <typename T>
  T* echoAlloc(size_t count) {
    typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(allocator);
    T* ptr = alloc.allocate(count);
    // Initialize to zero (equivalent to calloc behavior)
    for (size_t i = 0; i < count; ++i) {
      std::allocator_traits<typename std::allocator_traits<Allocator>::template rebind_alloc<T>>::construct(alloc, ptr + i);
    }
    return ptr;
  }

  /**
   * @brief Deallocate memory using custom allocator
   * @tparam T Type of objects to deallocate
   * @param ptr Pointer to memory to deallocate
   * @param count Number of objects (currently unused but kept for API compatibility)
   * @note For POD types used in speexdsp, count parameter is not strictly required
   */
  template <typename T>
  void echoFree(T* ptr, size_t count = 0) {
    if (ptr) {
      typename std::allocator_traits<Allocator>::template rebind_alloc<T> alloc(allocator);
      // Note: count is not used since we can't track allocation size
      // This is acceptable for POD types as we're using with speexdsp
      alloc.deallocate(ptr, count);
    }
  }

  inline void echoWarning(const char* str) {
    LOGW("EchoCanceller Warning: %s", str);
  }

  inline void echoFatal(const char* str) {
    LOGE("EchoCanceller Error: %s", str);
  }

  inline float spxSqrt(float x) { return sqrtf(x); }

  inline echo_int16_t spxIlog2(echo_uint32_t x) {
    int r = 0;
    if (x >= (echo_int32_t)65536) {
      x >>= 16;
      r += 16;
    }
    if (x >= 256) {
      x >>= 8;
      r += 8;
    }
    if (x >= 16) {
      x >>= 4;
      r += 4;
    }
    if (x >= 4) {
      x >>= 2;
      r += 2;
    }
    if (x >= 2) {
      r += 1;
    }
    return r;
  }

  /**
   * @brief Compute exponential function
   * @param x Input value
   * @return e raised to the power of x
   */
  inline float spxExp(float x) { return expf(x); }

  /**
   * @brief Compute cosine function
   * @param x Input angle in radians
   * @return Cosine of x
   */
  inline float spxCos(float x) { return cosf(x); }

  /**
   * @brief Apply DC notch filter to remove DC offset
   * @param in Input signal
   * @param radius Filter radius parameter
   * @param out Output filtered signal
   * @param len Length of signal
   * @param mem Filter memory state (2 values)
   * @param stride Stride for accessing input array
   */
  inline void filterDcNotch16(const echo_int16_t* in, echo_word16_t radius,
                              echo_word16_t* out, int len, echo_mem_t* mem,
                              int stride) {
    echo_word16_t den2 = radius * radius + .7f * (1 - radius) * (1 - radius);
    for (int i = 0; i < len; i++) {
      echo_word16_t vin = in[i * stride];
      echo_word32_t vout = mem[0] + vin;
      mem[0] = mem[1] + 2 * (-vin + radius * vout);
      mem[1] = vin - den2 * vout;
      out[i] = radius * vout;
    }
  }

  /**
   * @brief Compute inner product of two vectors
   * @param x First input vector
   * @param y Second input vector
   * @param len Length of vectors
   * @return Sum of element-wise products
   */
  inline echo_word32_t mdfInnerProd(const echo_word16_t* x, const echo_word16_t* y,
                                   int len) {
    float sum = 0;
    for (int i = 0; i < len; i++) {
      sum += x[i] * y[i];
    }
    return sum;
  }

  /**
   * @brief Compute power spectrum from FFT output
   * @param X FFT output in packed format
   * @param ps Output power spectrum
   * @param N FFT size
   */
  inline void powerSpectrum(const echo_word16_t* X, echo_word32_t* ps, int N) {
    ps[0] = X[0] * X[0];
    for (int i = 1, j = 1; i < N - 1; i += 2, j++) {
      ps[j] = X[i] * X[i] + X[i + 1] * X[i + 1];
    }
    ps[N / 2] = X[N - 1] * X[N - 1];
  }

  /**
   * @brief Accumulate power spectrum from FFT output
   * @param X FFT output in packed format
   * @param ps Accumulated power spectrum (updated in place)
   * @param N FFT size
   */
  inline void powerSpectrumAccum(const echo_word16_t* X, echo_word32_t* ps,
                                 int N) {
    ps[0] += X[0] * X[0];
    for (int i = 1, j = 1; i < N - 1; i += 2, j++) {
      ps[j] += X[i] * X[i] + X[i + 1] * X[i + 1];
    }
    ps[N / 2] += X[N - 1] * X[N - 1];
  }

  /**
   * @brief Accumulate spectral multiplication across multiple frames
   * @param X Input spectrum (M frames)
   * @param Y Filter weights in frequency domain
   * @param acc Output accumulated result
   * @param N FFT size
   * @param M Number of frames to accumulate
   */
  inline void spectralMulAccum(const echo_word16_t* X, const echo_word32_t* Y,
                               echo_word16_t* acc, int N, int M) {
    for (int i = 0; i < N; i++) acc[i] = 0;

    for (int j = 0; j < M; j++) {
      acc[0] += X[0] * Y[0];
      for (int i = 1; i < N - 1; i += 2) {
        acc[i] += (X[i] * Y[i] - X[i + 1] * Y[i + 1]);
        acc[i + 1] += (X[i + 1] * Y[i] + X[i] * Y[i + 1]);
      }
      acc[N - 1] += X[N - 1] * Y[N - 1];
      X += N;
      Y += N;
    }
  }

  /**
   * @brief Compute weighted spectral multiplication with conjugate
   * @param w Weight array for each frequency bin
   * @param p Global scaling factor
   * @param X First input spectrum
   * @param Y Second input spectrum (conjugated)
   * @param prod Output product
   * @param N FFT size
   */
  inline void weightedSpectralMulConj(const echo_float_t* w, const echo_float_t p,
                                      const echo_word16_t* X,
                                      const echo_word16_t* Y, echo_word32_t* prod,
                                      int N) {
    echo_float_t W;
    W = p * w[0];
    prod[0] = W * ((int32_t)X[0] * Y[0]);
    for (int i = 1, j = 1; i < N - 1; i += 2, j++) {
      W = p * w[j];
      prod[i] = W * ((int32_t)(X[i] * Y[i] + X[i + 1] * Y[i + 1]));
      prod[i + 1] = W * ((int32_t)(-X[i + 1] * Y[i] + X[i] * Y[i + 1]));
    }
    W = p * w[N / 2];
    prod[N - 1] = W * ((int32_t)X[N - 1] * Y[N - 1]);
  }

  /**
   * @brief Adjust proportional adaptation weights
   * @param W Filter weights in frequency domain
   * @param N FFT size
   * @param M Number of filter blocks
   * @param P Number of channels
   * @param prop Output proportional weights (normalized)
   */
  inline void mdfAdjustProp(const echo_word32_t* W, int N, int M, int P,
                            echo_word16_t* prop) {
    echo_word16_t max_sum = 1;
    echo_word32_t prop_sum = 1;

    for (int i = 0; i < M; i++) {
      echo_word32_t tmp = 1;
      for (int p = 0; p < P; p++) {
        for (int j = 0; j < N; j++) {
          float val = W[p * N * M + i * N + j];
          tmp += val * val;
        }
      }
      prop[i] = spxSqrt(tmp);
      if (prop[i] > max_sum) max_sum = prop[i];
    }

    for (int i = 0; i < M; i++) {
      prop[i] += 0.1f * max_sum;
      prop_sum += prop[i];
    }

    for (int i = 0; i < M; i++) {
      prop[i] = 0.99f * prop[i] / prop_sum;
    }
  }

  /**
   * @brief Initialize multi-channel echo canceller state
   * @param frame_size Number of samples per frame
   * @param filter_length Total filter length in samples
   * @param nb_mic Number of microphone channels
   * @param nb_speakers Number of speaker channels
   * @return Pointer to initialized echo state, or nullptr on failure
   */
  EchoState* echoStateInitMc(int frame_size, int filter_length, int nb_mic,
                                  int nb_speakers) {
    int N = frame_size * 2;
    int M = (filter_length + frame_size - 1) / frame_size;
    int C = nb_mic;
    int K = nb_speakers;

    EchoState* st = new EchoState();
    if (!st) return nullptr;

    st->K = K;
    st->C = C;
    st->frame_size = frame_size;
    st->window_size = N;
    st->M = M;
    st->cancel_count = 0;
    st->sum_adapt = 0;
    st->saturated = 0;
    st->screwed_up = 0;
    st->sampling_rate = 8000;
    st->spec_average = st->frame_size / (float)st->sampling_rate;
    st->beta0 = (2.0f * st->frame_size) / st->sampling_rate;
    st->beta_max = (.5f * st->frame_size) / st->sampling_rate;
    st->leak_estimate = 0;

    // Allocate buffers
    st->e = echoAlloc<echo_word16_t>(C * N);
    st->x = echoAlloc<echo_word16_t>(K * N);
    st->input = echoAlloc<echo_word16_t>(C * st->frame_size);
    st->y = echoAlloc<echo_word16_t>(C * N);
    st->last_y = echoAlloc<echo_word16_t>(C * N);
    st->Yf = echoAlloc<echo_word32_t>(st->frame_size + 1);
    st->Rf = echoAlloc<echo_word32_t>(st->frame_size + 1);
    st->Xf = echoAlloc<echo_word32_t>(st->frame_size + 1);
    st->Yh = echoAlloc<echo_word32_t>(st->frame_size + 1);
    st->Eh = echoAlloc<echo_word32_t>(st->frame_size + 1);
    st->X = echoAlloc<echo_word16_t>(K * (M + 1) * N);
    st->Y = echoAlloc<echo_word16_t>(C * N);
    st->E = echoAlloc<echo_word16_t>(C * N);
    st->W = echoAlloc<echo_word32_t>(C * K * M * N);

#ifdef TWO_PATH
    st->foreground = echoAlloc<echo_word16_t>(M * N * C * K);
#endif

    st->PHI = echoAlloc<echo_word32_t>(N);
    st->power = echoAlloc<echo_word32_t>(frame_size + 1);
    st->power_1 = echoAlloc<echo_float_t>(frame_size + 1);
    st->window = echoAlloc<echo_word16_t>(N);
    st->prop = echoAlloc<echo_word16_t>(M);
    st->wtmp = echoAlloc<echo_word16_t>(N);

    // Initialize window
    for (int i = 0; i < N; i++)
      st->window[i] = .5f - .5f * cosf(2 * M_PI * i / N);

    // Initialize power_1
    for (int i = 0; i <= st->frame_size; i++) st->power_1[i] = FLOAT_ONE;

    // Initialize W
    for (int i = 0; i < N * M * K * C; i++) st->W[i] = 0;

    // Initialize prop
    {
      echo_word32_t sum = 0;
      float decay = expf(-2.4f / M);
      st->prop[0] = .7f;
      sum = st->prop[0];
      for (int i = 1; i < M; i++) {
        st->prop[i] = st->prop[i - 1] * decay;
        sum += st->prop[i];
      }
      for (int i = M - 1; i >= 0; i--) {
        st->prop[i] = .8f * st->prop[i] / sum;
      }
    }

    st->memX = echoAlloc<echo_word16_t>(K);
    st->memD = echoAlloc<echo_word16_t>(C);
    st->memE = echoAlloc<echo_word16_t>(C);
    st->preemph = .9f;

    if (st->sampling_rate < 12000)
      st->notch_radius = .9f;
    else if (st->sampling_rate < 24000)
      st->notch_radius = .982f;
    else
      st->notch_radius = .992f;

    st->notch_mem = echoAlloc<echo_mem_t>(2 * C);
    st->adapted = 0;
    st->Pey = st->Pyy = FLOAT_ONE;

#ifdef TWO_PATH
    st->Davg1 = st->Davg2 = 0;
    st->Dvar1 = st->Dvar2 = FLOAT_ZERO;
#endif

    st->play_buf =
        echoAlloc<echo_int16_t>(K * (PLAYBACK_DELAY + 1) * st->frame_size);
    st->play_buf_pos = PLAYBACK_DELAY * st->frame_size;
    st->play_buf_started = 0;

    st->fft_table = nullptr;

    return st;
  }

  /**
   * @brief Core echo cancellation implementation
   * 
   * Implements the complete MDF echo cancellation algorithm including:
   * - Pre-processing (DC notch filter, pre-emphasis)
   * - FFT transformation of input signals
   * - Adaptive filter update in frequency domain
   * - Echo prediction and subtraction
   * - Two-path filter switching for double-talk handling
   * - Post-processing (de-emphasis)
   * 
   * @param st Echo state structure containing all internal state
   * @param in Input signal from microphones (interleaved if multi-channel)
   * @param far_end Reference signal from speakers (interleaved if multi-channel)
   * @param out Output signal with echo removed (interleaved if multi-channel)
   */
  inline void echoCancellationImpl(EchoState* st, const echo_int16_t* in,
                                   const echo_int16_t* far_end,
                                   echo_int16_t* out) {
    int N = st->window_size;
    int M = st->M;
    int C = st->C;
    int K = st->K;

    st->cancel_count++;
    float ss = .35f / M;
    float ss_1 = 1 - ss;

    // Apply notch filter and pre-emphasis to input
    for (int chan = 0; chan < C; chan++) {
      filterDcNotch16(in + chan, st->notch_radius,
                      st->input + chan * st->frame_size, st->frame_size,
                      st->notch_mem + 2 * chan, C);

      for (int i = 0; i < st->frame_size; i++) {
        echo_word32_t tmp32 =
            st->input[chan * st->frame_size + i] - st->preemph * st->memD[chan];
        st->memD[chan] = st->input[chan * st->frame_size + i];
        st->input[chan * st->frame_size + i] = tmp32;
      }
    }

    // Process far-end signal
    for (int speak = 0; speak < K; speak++) {
      std::memmove(&st->x[speak * N], 
                   &st->x[speak * N + st->frame_size],
                   st->frame_size * sizeof(echo_word16_t));
      for (int i = 0; i < st->frame_size; i++) {
        echo_word32_t tmp32 =
            far_end[i * K + speak] - st->preemph * st->memX[speak];
        st->x[speak * N + i + st->frame_size] = tmp32;
        st->memX[speak] = far_end[i * K + speak];
      }
    }

    // Shift memory and compute FFT of far-end
    for (int speak = 0; speak < K; speak++) {
      for (int j = M - 1; j >= 0; j--) {
        std::memmove(&st->X[(j + 1) * N * K + speak * N],
                     &st->X[j * N * K + speak * N],
                     N * sizeof(echo_word16_t));
      }
      echo_fft<Allocator>(st->fft_table, st->x + speak * N, &st->X[speak * N]);
    }

    // Compute power spectrum of far-end
    echo_word32_t Sxx = 0;
    for (int speak = 0; speak < K; speak++) {
      Sxx += mdfInnerProd(st->x + speak * N + st->frame_size,
                          st->x + speak * N + st->frame_size, st->frame_size);
      powerSpectrumAccum(st->X + speak * N, st->Xf, N);
    }

    // Compute foreground filter output and residual
    echo_word32_t Sff = 0;
    for (int chan = 0; chan < C; chan++) {
#ifdef TWO_PATH
      spectralMulAccum(st->X, st->foreground + chan * N * K * M,
                       st->Y + chan * N, N, M * K);
      echo_ifft<Allocator>(st->fft_table, st->Y + chan * N, st->e + chan * N);
      for (int i = 0; i < st->frame_size; i++)
        st->e[chan * N + i] = st->input[chan * st->frame_size + i] -
                              st->e[chan * N + i + st->frame_size];
      Sff += mdfInnerProd(st->e + chan * N, st->e + chan * N, st->frame_size);
#endif
    }

    // Adjust proportional adaptation
    if (st->adapted) mdfAdjustProp(st->W, N, M, C * K, st->prop);

    // Compute weight gradient
    if (st->saturated == 0) {
      for (int chan = 0; chan < C; chan++) {
        for (int speak = 0; speak < K; speak++) {
          for (int j = M - 1; j >= 0; j--) {
            weightedSpectralMulConj(st->power_1, st->prop[j],
                                    &st->X[(j + 1) * N * K + speak * N],
                                    st->E + chan * N, st->PHI, N);
            for (int i = 0; i < N; i++)
              st->W[chan * N * K * M + j * N * K + speak * N + i] += st->PHI[i];
          }
        }
      }
    } else {
      st->saturated--;
    }

    // Update weights (AUMDF)
    for (int chan = 0; chan < C; chan++) {
      for (int speak = 0; speak < K; speak++) {
        for (int j = 0; j < M; j++) {
          if (j == 0 || st->cancel_count % (M - 1) == j - 1) {
            echo_ifft<Allocator>(
                st->fft_table, &st->W[chan * N * K * M + j * N * K + speak * N],
                st->wtmp);
            for (int i = st->frame_size; i < N; i++) st->wtmp[i] = 0;
            echo_fft<Allocator>(
                st->fft_table, st->wtmp,
                &st->W[chan * N * K * M + j * N * K + speak * N]);
          }
        }
      }
    }

    // Initialize spectrum buffers
    for (int i = 0; i <= st->frame_size; i++)
      st->Rf[i] = st->Yf[i] = st->Xf[i] = 0;

    echo_word32_t Dbf = 0;
    echo_word32_t See = 0;

#ifdef TWO_PATH
    // Compute background filter output
    for (int chan = 0; chan < C; chan++) {
      spectralMulAccum(st->X, st->W + chan * N * K * M, st->Y + chan * N, N,
                       M * K);
      echo_ifft<Allocator>(st->fft_table, st->Y + chan * N, st->y + chan * N);
      for (int i = 0; i < st->frame_size; i++)
        st->e[chan * N + i] = st->e[chan * N + i + st->frame_size] -
                              st->y[chan * N + i + st->frame_size];
      Dbf +=
          10 + mdfInnerProd(st->e + chan * N, st->e + chan * N, st->frame_size);
      for (int i = 0; i < st->frame_size; i++)
        st->e[chan * N + i] = st->input[chan * st->frame_size + i] -
                              st->y[chan * N + i + st->frame_size];
      See += mdfInnerProd(st->e + chan * N, st->e + chan * N, st->frame_size);
    }
#endif

#ifndef TWO_PATH
    Sff = See;
#endif

#ifdef TWO_PATH
    // Two-path filter logic
    st->Davg1 = .6f * st->Davg1 + .4f * (Sff - See);
    st->Davg2 = .85f * st->Davg2 + .15f * (Sff - See);
    st->Dvar1 = FLOAT_ADD(FLOAT_MULT(VAR1_SMOOTH, st->Dvar1),
                          FLOAT_MUL32U(.4f * Sff, .4f * Dbf));
    st->Dvar2 = FLOAT_ADD(FLOAT_MULT(VAR2_SMOOTH, st->Dvar2),
                          FLOAT_MUL32U(.15f * Sff, .15f * Dbf));

    int update_foreground = 0;
    if (FLOAT_GT(FLOAT_MUL32U(Sff - See, fabsf(Sff - See)),
                 FLOAT_MUL32U(Sff, Dbf)))
      update_foreground = 1;
    else if (FLOAT_GT(FLOAT_MUL32U(st->Davg1, fabsf(st->Davg1)),
                      FLOAT_MULT(VAR1_UPDATE, st->Dvar1)))
      update_foreground = 1;
    else if (FLOAT_GT(FLOAT_MUL32U(st->Davg2, fabsf(st->Davg2)),
                      FLOAT_MULT(VAR2_UPDATE, st->Dvar2)))
      update_foreground = 1;

    if (update_foreground) {
      st->Davg1 = st->Davg2 = 0;
      st->Dvar1 = st->Dvar2 = FLOAT_ZERO;
      std::memcpy(st->foreground, st->W, N * M * C * K * sizeof(echo_word16_t));
      for (int chan = 0; chan < C; chan++)
        for (int i = 0; i < st->frame_size; i++)
          st->e[chan * N + i + st->frame_size] =
              st->window[i + st->frame_size] *
                  st->e[chan * N + i + st->frame_size] +
              st->window[i] * st->y[chan * N + i + st->frame_size];
    } else {
      int reset_background = 0;
      if (FLOAT_GT(FLOAT_MUL32U(-(Sff - See), fabsf(Sff - See)),
                   FLOAT_MULT(VAR_BACKTRACK, FLOAT_MUL32U(Sff, Dbf))))
        reset_background = 1;
      if (FLOAT_GT(FLOAT_MUL32U(-st->Davg1, fabsf(st->Davg1)),
                   FLOAT_MULT(VAR_BACKTRACK, st->Dvar1)))
        reset_background = 1;
      if (FLOAT_GT(FLOAT_MUL32U(-st->Davg2, fabsf(st->Davg2)),
                   FLOAT_MULT(VAR_BACKTRACK, st->Dvar2)))
        reset_background = 1;

      if (reset_background) {
        std::memcpy(st->W, st->foreground, N * M * C * K * sizeof(echo_word32_t));
        for (int chan = 0; chan < C; chan++) {
          for (int i = 0; i < st->frame_size; i++)
            st->y[chan * N + i + st->frame_size] =
                st->e[chan * N + i + st->frame_size];
          for (int i = 0; i < st->frame_size; i++)
            st->e[chan * N + i] = st->input[chan * st->frame_size + i] -
                                  st->y[chan * N + i + st->frame_size];
        }
        See = Sff;
        st->Davg1 = st->Davg2 = 0;
        st->Dvar1 = st->Dvar2 = FLOAT_ZERO;
      }
    }
#endif

    echo_word32_t Sey = 0, Syy = 0, Sdd = 0;
    for (int chan = 0; chan < C; chan++) {
      // Compute output with de-emphasis
      for (int i = 0; i < st->frame_size; i++) {
        echo_word32_t tmp_out;
#ifdef TWO_PATH
        tmp_out = st->input[chan * st->frame_size + i] -
                  st->e[chan * N + i + st->frame_size];
#else
        tmp_out = st->input[chan * st->frame_size + i] -
                  st->y[chan * N + i + st->frame_size];
#endif
        tmp_out = tmp_out + st->preemph * st->memE[chan];
        if (in[i * C + chan] <= -32000 || in[i * C + chan] >= 32000) {
          if (st->saturated == 0) st->saturated = 1;
        }
        out[i * C + chan] = WORD2INT(tmp_out);
        st->memE[chan] = tmp_out;
      }

      // Prepare error signal for filter update
      for (int i = 0; i < st->frame_size; i++) {
        st->e[chan * N + i + st->frame_size] = st->e[chan * N + i];
        st->e[chan * N + i] = 0;
      }

      // Compute correlations
      Sey += mdfInnerProd(st->e + chan * N + st->frame_size,
                          st->y + chan * N + st->frame_size, st->frame_size);
      Syy += mdfInnerProd(st->y + chan * N + st->frame_size,
                          st->y + chan * N + st->frame_size, st->frame_size);
      Sdd += mdfInnerProd(st->input + chan * st->frame_size,
                          st->input + chan * st->frame_size, st->frame_size);

      // Convert error to frequency domain
      echo_fft<Allocator>(st->fft_table, st->e + chan * N, st->E + chan * N);

      for (int i = 0; i < st->frame_size; i++) st->y[i + chan * N] = 0;
      echo_fft<Allocator>(st->fft_table, st->y + chan * N, st->Y + chan * N);

      // Compute power spectra
      powerSpectrumAccum(st->E + chan * N, st->Rf, N);
      powerSpectrumAccum(st->Y + chan * N, st->Yf, N);
    }

    // Sanity checks
    if (!(Syy >= 0 && Sxx >= 0 && See >= 0) ||
        !(Sff < N * 1e9f && Syy < N * 1e9f && Sxx < N * 1e9f)) {
      st->screwed_up += 50;
      for (int i = 0; i < st->frame_size * C; i++) out[i] = 0;
    } else if (Sff > Sdd + N * 10000.0f) {
      st->screwed_up++;
    } else {
      st->screwed_up = 0;
    }

    if (st->screwed_up >= 50) {
      echoWarning("Echo canceller reset");
      reset();
      return;
    }

    See = MAX32(See, N * 100.0f);

    for (int speak = 0; speak < K; speak++) {
      Sxx += mdfInnerProd(st->x + speak * N + st->frame_size,
                          st->x + speak * N + st->frame_size, st->frame_size);
      powerSpectrumAccum(st->X + speak * N, st->Xf, N);
    }

    // Smooth far-end energy
    for (int j = 0; j <= st->frame_size; j++)
      st->power[j] = ss_1 * st->power[j] + 1 + ss * st->Xf[j];

    // Compute filtered spectra and correlations
    echo_float_t Pey = FLOAT_ZERO, Pyy = FLOAT_ZERO;
    for (int j = st->frame_size; j >= 0; j--) {
      echo_float_t Eh = PSEUDOFLOAT(st->Rf[j] - st->Eh[j]);
      echo_float_t Yh = PSEUDOFLOAT(st->Yf[j] - st->Yh[j]);
      Pey = FLOAT_ADD(Pey, FLOAT_MULT(Eh, Yh));
      Pyy = FLOAT_ADD(Pyy, FLOAT_MULT(Yh, Yh));
      st->Eh[j] =
          (1 - st->spec_average) * st->Eh[j] + st->spec_average * st->Rf[j];
      st->Yh[j] =
          (1 - st->spec_average) * st->Yh[j] + st->spec_average * st->Yf[j];
    }

    Pyy = FLOAT_SQRT(Pyy);
    Pey = FLOAT_DIVU(Pey, Pyy);

    // Compute correlation update rate
    echo_word32_t tmp32 = st->beta0 * Syy;
    if (tmp32 > st->beta_max * See) tmp32 = st->beta_max * See;
    echo_float_t alpha = tmp32 / See;
    echo_float_t alpha_1 = FLOAT_SUB(FLOAT_ONE, alpha);

    st->Pey = FLOAT_ADD(FLOAT_MULT(alpha_1, st->Pey), FLOAT_MULT(alpha, Pey));
    st->Pyy = FLOAT_ADD(FLOAT_MULT(alpha_1, st->Pyy), FLOAT_MULT(alpha, Pyy));
    if (FLOAT_LT(st->Pyy, FLOAT_ONE)) st->Pyy = FLOAT_ONE;
    if (FLOAT_LT(st->Pey, FLOAT_MULT(MIN_LEAK, st->Pyy)))
      st->Pey = FLOAT_MULT(MIN_LEAK, st->Pyy);
    if (FLOAT_GT(st->Pey, st->Pyy)) st->Pey = st->Pyy;

    st->leak_estimate =
        FLOAT_EXTRACT16(FLOAT_SHL(FLOAT_DIVU(st->Pey, st->Pyy), 14));
    if (st->leak_estimate > 16383)
      st->leak_estimate = 32767;
    else
      st->leak_estimate = st->leak_estimate * 2;

    // Compute RER
    echo_word16_t RER;
    RER = (.0001f * Sxx + 3.f * st->leak_estimate * Syy) / See;
    if (RER < Sey * Sey / (1 + See * Syy)) RER = Sey * Sey / (1 + See * Syy);
    if (RER > .5f) RER = .5f;

    if (!st->adapted && st->sum_adapt > M &&
        st->leak_estimate * Syy > .03f * Syy) {
      st->adapted = 1;
    }

    if (st->adapted) {
      for (int i = 0; i <= st->frame_size; i++) {
        echo_word32_t r = st->leak_estimate * st->Yf[i];
        echo_word32_t e = st->Rf[i] + 1;
        if (r > .5f * e) r = .5f * e;
        r = .7f * r + .3f * (RER * e);
        st->power_1[i] = r / (e * (st->power[i] + 10));
      }
    } else {
      echo_word16_t adapt_rate = 0;
      if (Sxx > N * 1000.0f) {
        tmp32 = .25f * Sxx;
        if (tmp32 > .25f * See) tmp32 = .25f * See;
        adapt_rate = tmp32 / See;
      }
      for (int i = 0; i <= st->frame_size; i++)
        st->power_1[i] = adapt_rate / (st->power[i] + 10);
      st->sum_adapt = st->sum_adapt + adapt_rate;
    }

    std::memmove(st->last_y, 
                 &st->last_y[st->frame_size],
                 st->frame_size * sizeof(echo_word16_t));
    if (st->adapted) {
      for (int i = 0; i < st->frame_size; i++)
        st->last_y[st->frame_size + i] = in[i] - out[i];
    }
  }
};

// ============================================================================
// FFT Implementation
// ============================================================================
/**
 * @brief Initialize FFT state
 * @tparam Allocator Custom allocator type for memory management
 * @param size FFT size (number of points)
 * @param driver Pointer to FFT driver implementing AudioFFTBase interface
 * @param alloc Allocator instance for memory management
 * @return Opaque pointer to FFT state, or nullptr on failure
 */
template <typename Allocator>
inline void* echo_fft_init(int size, AudioFFTBase* driver, const Allocator& alloc) {
  if (!driver) {
    return nullptr;
  }
  
  // Configure FFT with the required size
  AudioFFTConfig cfg;
  cfg.length = size;
  cfg.rxtx_mode = TX_MODE;  // We need both FFT and IFFT capabilities
  
  if (!driver->begin(cfg)) {
    return nullptr;
  }
  return new fft_state<Allocator>(size, driver, alloc);
}

/**
 * @brief Destroy FFT state and release resources
 * @tparam Allocator Custom allocator type (must match the one used in echo_fft_init)
 * @param table Opaque pointer to FFT state returned by echo_fft_init
 */
template <typename Allocator = std::allocator<uint8_t>>
inline void echo_fft_destroy(void* table) {
  if (table) {
    auto* st = static_cast<fft_state<Allocator>*>(table);
    st->driver->end();
    delete st;
  }
}

/**
 * @brief Perform forward FFT
 * @tparam Allocator Custom allocator type (must match the one used in echo_fft_init)
 * @param table Opaque pointer to FFT state
 * @param in Input time-domain signal (size N)
 * @param out Output frequency-domain signal in packed format (size N)
 *            Format: [DC, real1, imag1, real2, imag2, ..., Nyquist]
 */
template <typename Allocator = std::allocator<uint8_t>>
inline void echo_fft(void* table, echo_word16_t* in,
                    echo_word16_t* out) {
  auto* st = static_cast<fft_state<Allocator>*>(table);
  if (!st || !st->driver) return;

  // Set input values
  for (int i = 0; i < st->N; i++) {
    st->driver->setValue(i, in[i] / (float)st->N);
  }

  // Perform FFT
  st->driver->fft();

  // Get output in packed format: out[0]=real[0], out[1]=real[1],
  // out[2]=img[1],
  // ...
  out[0] = st->driver->getValue(0);  // DC component
  for (int i = 1; i < st->N - 1; i += 2) {
    int bin = (i + 1) / 2;
    float real, img;
    st->driver->getBin(bin, real, img);
    out[i] = real;
    out[i + 1] = img;
  }
  out[st->N - 1] = st->driver->getValue(st->N / 2);  // Nyquist
}

/**
 * @brief Perform inverse FFT
 * @tparam Allocator Custom allocator type (must match the one used in echo_fft_init)
 * @param table Opaque pointer to FFT state
 * @param in Input frequency-domain signal in packed format (size N)
 *           Format: [DC, real1, imag1, real2, imag2, ..., Nyquist]
 * @param out Output time-domain signal (size N)
 */
template <typename Allocator = std::allocator<uint8_t>>
inline void echo_ifft(void* table, echo_word16_t* in,
                     echo_word16_t* out) {
  auto* st = static_cast<fft_state<Allocator>*>(table);
  if (!st || !st->driver || !st->driver->isReverseFFT()) return;

  // Set bins from packed format
  st->driver->setBin(0, in[0], 0);
  for (int i = 1; i < st->N - 1; i += 2) {
    int bin = (i + 1) / 2;
    st->driver->setBin(bin, in[i], in[i + 1]);
  }
  st->driver->setBin(st->N / 2, in[st->N - 1], 0);

  // Perform inverse FFT
  st->driver->rfft();

  // Get output
  for (int i = 0; i < st->N; i++) {
    out[i] = st->driver->getValue(i);
  }
}

}  // namespace audio_tools
