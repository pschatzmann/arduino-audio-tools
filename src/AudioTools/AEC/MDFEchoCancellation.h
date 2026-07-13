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

#include "MDFEchoCancellationConfig.h"
#include "PseudoFloat.h"
#include "AudioTools/FFT/AudioFFT.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h"

// Control requests
#define ECHO_GET_FRAME_SIZE 3
#define ECHO_SET_SAMPLING_RATE 24
#define ECHO_GET_SAMPLING_RATE 25
#define ECHO_GET_IMPULSE_RESPONSE_SIZE 27
#define ECHO_GET_IMPULSE_RESPONSE 29

// Converts a de-emphasized sample back to a clamped 16-bit PCM value.
// Independent of the SampleType template parameter below -- this always
// produces the final echo_int16_t output, regardless of which numeric type
// was used internally to get there.
#define WORD2INT(x) \
  ((x) < -32767.5f  \
       ? -32768     \
       : ((x) > 32766.5f ? 32767 : (echo_int16_t)floorf(.5f + (x))))

namespace audio_tools {

// Type definitions for the fixed-width PCM/control values that are always
// concrete integers, regardless of the internal DSP numeric representation
// chosen via SampleType (see MDFFloat / MDFFixedPoint below).
using echo_int16_t = int16_t;
using echo_uint16_t = uint16_t;
using echo_int32_t = int32_t;
using echo_uint32_t = uint32_t;

/**
 * @brief Selects `float` as MDFEchoCancellation's internal numeric
 * representation for sample/spectrum arrays (word16_t/word32_t) and
 * wide-dynamic-range control state (float_t). This is the default, and the
 * one that has been verified to converge (see test_mdf_converges in
 * tests-cmake/stt/stt_test.cpp) -- prefer it unless you specifically need
 * to avoid hardware floating-point instructions (e.g. no FPU).
 */
struct MDFFloat {
  using word16_t = float;
  using word32_t = float;
  using float_t = float;
};

/**
 * @brief Selects PseudoFloat (see PseudoFloat.h) as MDFEchoCancellation's
 * internal numeric representation, so the algorithm runs on integer
 * mantissa/exponent arithmetic instead of native float instructions --
 * useful on microcontrollers without an FPU. PseudoFloat's arithmetic is
 * verified against native float directly (see the PseudoFloat tests in
 * tests-cmake/stt/stt_test.cpp), and MDFEchoCancellation<MDFFixedPoint> is
 * smoke-tested to run without crashing/NaN and to converge similarly to
 * MDFFloat on the same synthetic signal -- but it has not received the
 * same depth of numerical scrutiny as MDFFloat, so treat it as less
 * battle-tested. Despite the name, this is not a classic narrow Q15
 * fixed-point format (that would over/underflow across this algorithm's
 * actual value range -- see PseudoFloat's class doc for why).
 */
struct MDFFixedPoint {
  using word16_t = PseudoFloat;
  using word32_t = PseudoFloat;
  using float_t = PseudoFloat;
};

/**
 * @brief Internal echo canceller state structure
 *
 * Contains all internal state variables and buffers for the MDF (Multi-Delay
 * block Frequency adaptive filter) echo cancellation algorithm. This structure
 * should never be accessed directly by users; use the EchoCanceller class
 * methods instead.
 *
 * @tparam SampleType MDFFloat (default) or MDFFixedPoint -- selects the
 * numeric type used for the arrays/state below (see either struct's doc).
 *
 * @warning Direct access to this structure is not recommended and may break
 * encapsulation.
 */
template <typename SampleType>
struct EchoState {
  using Word16 = typename SampleType::word16_t;
  using Word32 = typename SampleType::word32_t;
  using Num = typename SampleType::float_t;
  using Mem = Word32;

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
  Word16 spec_average;
  Word16 beta0;
  Word16 beta_max;
  Word32 sum_adapt;
  Word16 leak_estimate;

  Word16* e;     /* scratch */
  Word16* x;     /* Far-end input buffer (2N) */
  Word16* X;     /* Far-end buffer (M+1 frames) in frequency domain */
  Word16* input; /* scratch */
  Word16* y;     /* scratch */
  Word16* last_y;
  Word16* Y; /* scratch */
  Word16* E;
  Word32* PHI; /* scratch */
  Word32* W;   /* (Background) filter weights */
#ifdef TWO_PATH
  Word16* foreground; /* Foreground filter weights */
  Word32
      Davg1; /* 1st recursive average of the residual power difference */
  Word32
      Davg2; /* 2nd recursive average of the residual power difference */
  Num Dvar1; /* Estimated variance of 1st estimator */
  Num Dvar2; /* Estimated variance of 2nd estimator */
#endif
  Word32* power;  /* Power of the far-end signal */
  Num* power_1; /* Inverse power of far-end */
  Word16* wtmp;   /* scratch */
  Word32* Rf;     /* scratch */
  Word32* Yf;     /* scratch */
  Word32* Xf;     /* scratch */
  Word32* Eh;
  Word32* Yh;
  Num Pey;
  Num Pyy;
  Word16* window;
  Word16* prop;
  void* fft_table;
  Word16 *memX, *memD, *memE;
  Word16 preemph;
  Word16 notch_radius;
  Mem* notch_mem;

  echo_int16_t* play_buf;
  int play_buf_pos;
  int play_buf_started;
};

/**
 * @brief FFT state management structure
 *
 * Manages FFT operations through the raw FFTDriver interface. This is a
 * small, fixed-layout struct (just a driver pointer and a size), so it's
 * allocated with plain new/delete rather than through the audio_tools
 * Allocator used for the (much larger) EchoState buffers -- consistent with
 * how EchoState itself is created (see echoStateInitMc()).
 */
struct fft_state {
  // The MDF algorithm needs synchronous per-sample setValue()/fft()/getValue()
  // access at a window size (2x frame size) that is independent of whatever
  // AudioFFTBase::config().length the caller set up. That low-level, single-
  // shot API only exists on FFTDriver (e.g. FFTDriverRealFFT), not on the
  // higher-level AudioFFTBase stream wrapper, so we operate on the raw driver
  // obtained via AudioFFTBase::driver().
  FFTDriver* driver; /**< Pointer to the raw FFT driver implementation */
  int N; /**< FFT size */

  /**
   * @brief Construct FFT state with specified size and driver
   * @param size FFT size (number of points)
   * @param drv Pointer to the raw FFT driver implementation
   */
  fft_state(int size, FFTDriver* drv) : driver(drv), N(size) {}
};

// Forward declarations: these are defined at the bottom of this file but
// referenced from inside MDFEchoCancellation below. echo_fft/echo_ifft are
// templated on the sample element type (T = SampleType::word16_t, i.e.
// float or PseudoFloat) since they convert to/from the FFT driver's plain
// float bins at the boundary.
inline void* echo_fft_init(int size, FFTDriver* driver);
inline void echo_fft_destroy(void* table);
template <typename T>
inline void echo_fft(void* table, T* in, T* out);
template <typename T>
inline void echo_ifft(void* table, T* in, T* out);

/**
 * @brief Acoustic echo canceller using MDF algorithm
 *
 * High-performance echo cancellation implementation based on the Multi-Delay
 * block Frequency adaptive filter (MDF) algorithm. Supports both single and
 * multi-channel configurations with customizable memory allocation. This is
 * the underlying algorithm used by MDFEchoCancellationStream -- use this
 * class directly (via capture()/playback(), or the self-contained cancel())
 * if you don't want the Stream-based wrapper.
 *
 * The echo canceller operates by:
 * - Learning the echo path between loudspeakers and microphones
 * - Adaptively filtering the playback signal to predict the echo
 * - Subtracting the predicted echo from the captured microphone signal
 *
 * Features:
 * - Header-only C++ implementation
 * - Custom Allocator support for embedded systems (see
 *   AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h -- e.g.
 *   AllocatorPSRAM/AllocatorESP32 to place the (fairly large) echo-state
 *   buffers off the default heap)
 * - Two-path filter for improved double-talk handling
 * - Multi-channel support (multiple microphones and speakers)
 *
 * @note This implementation requires an external FFT driver implementing
 *       the AudioFFTBase interface from AudioFFT.h (only AudioRealFFT has
 *       been verified -- see the Weaknesses note below).
 *
 * Example usage:
 * @code
 * AudioRealFFT fftDriver;
 * auto cfg = fftDriver.defaultConfig(RXTX_MODE);
 * cfg.length = 256;           // frame size in samples
 * cfg.sample_rate = 16000;
 * fftDriver.begin(cfg);
 *
 * int filterLength = 1024;  // in samples
 * MDFEchoCancellation<> echo(filterLength, fftDriver);  // MDFFloat, uses DefaultAllocator
 * echo.playback(speakerFrame);          // frame_size samples
 * echo.capture(micFrame, outputFrame);  // frame_size samples each
 * @endcode
 *
 * Strengths (see MDFEchoCancellationStream for the fuller writeup):
 * - Block-frequency-domain adaptation handles much longer echo tails than a
 *   time-domain LMS filter (e.g. LMSEchoCancellationStream) at a fraction
 *   of the per-tap cost, and the two-path filter gives real protection
 *   against double-talk.
 * - Multi-channel (nbMic x nbSpeakers) support built in.
 * - Verified to converge strongly on a synthetic attenuated-echo signal,
 *   for both numeric backends (see test_mdf_converges<MDFFloat> and
 *   test_mdf_converges<MDFFixedPoint> in tests-cmake/stt/stt_test.cpp).
 * - The numeric representation used for internal arrays/state is a
 *   template parameter (@tparam SampleType, MDFFloat by default) chosen
 *   when you construct the object, e.g. `MDFEchoCancellation<MDFFixedPoint>`
 *   -- see MDFFloat/MDFFixedPoint's docs above for the tradeoff.
 *
 * Weaknesses:
 * - Only verified with the AudioRealFFT driver; other AudioFFTBase drivers
 *   have their own bin-access implementations that have not been audited.
 * - capture()/playback() must be called with exactly frame_size samples
 *   each; ensureInitialized() re-begin()s the FFT driver you pass in at a
 *   *different* size (2x frame_size) on first use, so dedicate that driver
 *   instance to this canceller.
 * - The two-path thresholds/heuristics are unmodified speexdsp defaults,
 *   only exercised here against a single synthetic tone with no near-end
 *   noise -- real acoustic echo is not yet validated.
 * - This file required extensive bug-fixing (missing forward declarations,
 *   word types hardcoded to int16_t/int32_t despite float-only arithmetic,
 *   a 0/0 = NaN in the leak estimate, a leftover fixed-point cast that
 *   zeroed the adaptive filter's gradient) to reach its current, tested
 *   state; treat further changes with real caution and re-run
 *   tests-cmake/stt afterwards.
 *
 * @tparam SampleType MDFFloat (default) or MDFFixedPoint.
 */
template <typename SampleType = MDFFloat>
class MDFEchoCancellation : public AudioStream {
 public:
  using Word16 = typename SampleType::word16_t;
  using Word32 = typename SampleType::word32_t;
  using Num = typename SampleType::float_t;
  using Mem = Word32;
  using State = EchoState<SampleType>;

  /** Initialize echo canceller with single channel (mono)
   * @param filterLength Length of echo cancellation filter in samples
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator used for the echo-state buffers (optional,
   *              defaults to the shared DefaultAllocator)
   */
  MDFEchoCancellation(int filterLength, AudioFFTBase& fftDriver, Allocator& alloc = DefaultAllocator)
      : fft_driver(&fftDriver), allocator(alloc),
        filter_length(filterLength), nb_mic(1), nb_speakers(1) {}

  /** Initialize echo canceller with multiple channels
   * @param filterLength Length of echo cancellation filter in samples
   * @param nbMic Number of microphone channels
   * @param nbSpeakers Number of speaker channels
   * @param fftDriver FFT driver instance from AudioFFT
   * @param alloc Allocator used for the echo-state buffers (optional,
   *              defaults to the shared DefaultAllocator)
   */
  MDFEchoCancellation(int filterLength, int nbMic, int nbSpeakers,
                AudioFFTBase& fftDriver, Allocator& alloc = DefaultAllocator)
      : fft_driver(&fftDriver), allocator(alloc),
        filter_length(filterLength), nb_mic(nbMic), nb_speakers(nbSpeakers) {}

  /** Destructor */
  ~MDFEchoCancellation() {
    if (!state) return;

    if (state->fft_table) echo_fft_destroy(state->fft_table);

    int N = state->window_size;
    int M = state->M;
    int C = state->C;
    int K = state->K;
    int frame_size = state->frame_size;

    echoFree(state->e, C * N);
    echoFree(state->x, K * N);
    echoFree(state->input, C * frame_size);
    echoFree(state->y, C * N);
    echoFree(state->last_y, C * N);
    echoFree(state->Yf, frame_size + 1);
    echoFree(state->Rf, frame_size + 1);
    echoFree(state->Xf, frame_size + 1);
    echoFree(state->Yh, frame_size + 1);
    echoFree(state->Eh, frame_size + 1);
    echoFree(state->X, K * (M + 1) * N);
    echoFree(state->Y, C * N);
    echoFree(state->E, C * N);
    echoFree(state->W, C * K * M * N);
#ifdef TWO_PATH
    echoFree(state->foreground, M * N * C * K);
#endif
    echoFree(state->PHI, N);
    echoFree(state->power, frame_size + 1);
    echoFree(state->power_1, frame_size + 1);
    echoFree(state->window, N);
    echoFree(state->prop, M);
    echoFree(state->wtmp, N);
    echoFree(state->memX, K);
    echoFree(state->memD, C);
    echoFree(state->memE, C);
    echoFree(state->notch_mem, 2 * C);
    echoFree(state->play_buf, K * (PLAYBACK_DELAY + 1) * frame_size);
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
      state->power_1[i] = 1.0f;
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
    state->Pey = state->Pyy = 1.0f;
#ifdef TWO_PATH
    state->Davg1 = state->Davg2 = 0;
    state->Dvar1 = state->Dvar2 = 0.0f;
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
          echo_ifft(state->fft_table, &state->W[j * N], state->wtmp);
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
  State* getState() { return state; }

 protected:
  State* state = nullptr; /**< Pointer to internal echo canceller state */
  AudioFFTBase* fft_driver; /**< Pointer to FFT driver instance */
  Allocator& allocator; /**< Allocator used for the echo-state buffers */
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
      state->fft_table = echo_fft_init(state->window_size, fft_driver->driver());
    }
    initialized = true;
  }

  /**
   * @brief Allocate a zero-initialized array of type T using the configured
   * Allocator (see AudioTools/CoreAudio/AudioBasic/Collections/Allocator.h)
   * @tparam T Type of objects to allocate
   * @param count Number of objects to allocate
   * @return Pointer to allocated and zero-initialized memory
   */
  template <typename T>
  T* echoAlloc(size_t count) {
    return allocator.createArray<T>(count);
  }

  /**
   * @brief Deallocate an array previously returned by echoAlloc()
   * @tparam T Type of objects to deallocate
   * @param ptr Pointer to memory to deallocate
   * @param count Number of objects originally allocated (must match the
   *              echoAlloc() call that produced ptr)
   */
  template <typename T>
  void echoFree(T* ptr, size_t count) {
    allocator.removeArray<T>(ptr, count);
  }

  inline void echoWarning(const char* str) {
    LOGW("EchoCanceller Warning: %s", str);
  }

  inline void echoFatal(const char* str) {
    LOGE("EchoCanceller Error: %s", str);
  }

  template <typename T>
  inline T spxSqrt(T x) { return T(sqrtf((float)x)); }

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
  inline void filterDcNotch16(const echo_int16_t* in, Word16 radius,
                              Word16* out, int len, Mem* mem,
                              int stride) {
    Word16 den2 = radius * radius + .7f * (1 - radius) * (1 - radius);
    for (int i = 0; i < len; i++) {
      Word16 vin = in[i * stride];
      Word32 vout = mem[0] + vin;
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
  inline Word32 mdfInnerProd(const Word16* x, const Word16* y,
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
  inline void powerSpectrum(const Word16* X, Word32* ps, int N) {
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
  inline void powerSpectrumAccum(const Word16* X, Word32* ps,
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
  inline void spectralMulAccum(const Word16* X, const Word32* Y,
                               Word16* acc, int N, int M) {
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
  inline void weightedSpectralMulConj(const Num* w, const Num p,
                                      const Word16* X,
                                      const Word16* Y, Word32* prod,
                                      int N) {
    // NOTE: the (int32_t) casts this code originally had here were only
    // correct in true fixed-point mode (X/Y as int16_t, needing widening
    // before multiplying to avoid 16-bit overflow). With Word16 /
    // Word32 now float (see the aliases above), those casts instead
    // truncated every frequency-domain sample to an integer before ever
    // multiplying it by anything -- e.g. a bin magnitude of 0.03 became 0,
    // silently zeroing out the filter's gradient. Removed.
    Num W;
    W = p * w[0];
    prod[0] = W * (X[0] * Y[0]);
    for (int i = 1, j = 1; i < N - 1; i += 2, j++) {
      W = p * w[j];
      prod[i] = W * (X[i] * Y[i] + X[i + 1] * Y[i + 1]);
      prod[i + 1] = W * (-X[i + 1] * Y[i] + X[i] * Y[i + 1]);
    }
    W = p * w[N / 2];
    prod[N - 1] = W * (X[N - 1] * Y[N - 1]);
  }

  /**
   * @brief Adjust proportional adaptation weights
   * @param W Filter weights in frequency domain
   * @param N FFT size
   * @param M Number of filter blocks
   * @param P Number of channels
   * @param prop Output proportional weights (normalized)
   */
  inline void mdfAdjustProp(const Word32* W, int N, int M, int P,
                            Word16* prop) {
    Word16 max_sum = 1;
    Word32 prop_sum = 1;

    for (int i = 0; i < M; i++) {
      Word32 tmp = 1;
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
  State* echoStateInitMc(int frame_size, int filter_length, int nb_mic,
                                  int nb_speakers) {
    int N = frame_size * 2;
    int M = (filter_length + frame_size - 1) / frame_size;
    int C = nb_mic;
    int K = nb_speakers;

    State* st = new State();
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
    st->e = echoAlloc<Word16>(C * N);
    st->x = echoAlloc<Word16>(K * N);
    st->input = echoAlloc<Word16>(C * st->frame_size);
    st->y = echoAlloc<Word16>(C * N);
    st->last_y = echoAlloc<Word16>(C * N);
    st->Yf = echoAlloc<Word32>(st->frame_size + 1);
    st->Rf = echoAlloc<Word32>(st->frame_size + 1);
    st->Xf = echoAlloc<Word32>(st->frame_size + 1);
    st->Yh = echoAlloc<Word32>(st->frame_size + 1);
    st->Eh = echoAlloc<Word32>(st->frame_size + 1);
    st->X = echoAlloc<Word16>(K * (M + 1) * N);
    st->Y = echoAlloc<Word16>(C * N);
    st->E = echoAlloc<Word16>(C * N);
    st->W = echoAlloc<Word32>(C * K * M * N);

#ifdef TWO_PATH
    st->foreground = echoAlloc<Word16>(M * N * C * K);
#endif

    st->PHI = echoAlloc<Word32>(N);
    st->power = echoAlloc<Word32>(frame_size + 1);
    st->power_1 = echoAlloc<Num>(frame_size + 1);
    st->window = echoAlloc<Word16>(N);
    st->prop = echoAlloc<Word16>(M);
    st->wtmp = echoAlloc<Word16>(N);

    // Initialize window
    for (int i = 0; i < N; i++)
      st->window[i] = .5f - .5f * cosf(2 * M_PI * i / N);

    // Initialize power_1
    for (int i = 0; i <= st->frame_size; i++) st->power_1[i] = 1.0f;

    // Initialize W
    for (int i = 0; i < N * M * K * C; i++) st->W[i] = 0;

    // Initialize prop
    {
      Word32 sum = 0;
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

    st->memX = echoAlloc<Word16>(K);
    st->memD = echoAlloc<Word16>(C);
    st->memE = echoAlloc<Word16>(C);
    st->preemph = .9f;

    if (st->sampling_rate < 12000)
      st->notch_radius = .9f;
    else if (st->sampling_rate < 24000)
      st->notch_radius = .982f;
    else
      st->notch_radius = .992f;

    st->notch_mem = echoAlloc<Mem>(2 * C);
    st->adapted = 0;
    st->Pey = st->Pyy = 1.0f;

#ifdef TWO_PATH
    st->Davg1 = st->Davg2 = 0;
    st->Dvar1 = st->Dvar2 = 0.0f;
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
  inline void echoCancellationImpl(State* st, const echo_int16_t* in,
                                   const echo_int16_t* far_end,
                                   echo_int16_t* out) {
    int N = st->window_size;
    int M = st->M;
    int C = st->C;
    int K = st->K;

    // Wide-dynamic-range tuning constants, as Num (float or PseudoFloat)
    // so they multiply/compare directly against Num-typed state below.
    const Num min_leak(0.005f);        // Minimum leak estimate for the adaptive filter
    const Num var1_smooth(0.36f);      // Smoothing coefficient, 1st variance estimator
    const Num var2_smooth(0.7225f);    // Smoothing coefficient, 2nd variance estimator
    const Num var1_update(0.5f);       // Update threshold, 1st variance estimator
    const Num var2_update(0.25f);      // Update threshold, 2nd variance estimator
    const Num var_backtrack(4.0f);     // Backtrack threshold for filter reset

    st->cancel_count++;
    float ss = .35f / M;
    float ss_1 = 1 - ss;

    // Apply notch filter and pre-emphasis to input
    for (int chan = 0; chan < C; chan++) {
      filterDcNotch16(in + chan, st->notch_radius,
                      st->input + chan * st->frame_size, st->frame_size,
                      st->notch_mem + 2 * chan, C);

      for (int i = 0; i < st->frame_size; i++) {
        Word32 tmp32 =
            st->input[chan * st->frame_size + i] - st->preemph * st->memD[chan];
        st->memD[chan] = st->input[chan * st->frame_size + i];
        st->input[chan * st->frame_size + i] = tmp32;
      }
    }

    // Process far-end signal
    for (int speak = 0; speak < K; speak++) {
      std::memmove(&st->x[speak * N], 
                   &st->x[speak * N + st->frame_size],
                   st->frame_size * sizeof(Word16));
      for (int i = 0; i < st->frame_size; i++) {
        Word32 tmp32 =
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
                     N * sizeof(Word16));
      }
      echo_fft(st->fft_table, st->x + speak * N, &st->X[speak * N]);
    }

    // Compute power spectrum of far-end
    Word32 Sxx = 0;
    for (int speak = 0; speak < K; speak++) {
      Sxx += mdfInnerProd(st->x + speak * N + st->frame_size,
                          st->x + speak * N + st->frame_size, st->frame_size);
      powerSpectrumAccum(st->X + speak * N, st->Xf, N);
    }

    // Compute foreground filter output and residual
    Word32 Sff = 0;
    for (int chan = 0; chan < C; chan++) {
#ifdef TWO_PATH
      spectralMulAccum(st->X, st->foreground + chan * N * K * M,
                       st->Y + chan * N, N, M * K);
      echo_ifft(st->fft_table, st->Y + chan * N, st->e + chan * N);
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
            echo_ifft(
                st->fft_table, &st->W[chan * N * K * M + j * N * K + speak * N],
                st->wtmp);
            for (int i = st->frame_size; i < N; i++) st->wtmp[i] = 0;
            echo_fft(
                st->fft_table, st->wtmp,
                &st->W[chan * N * K * M + j * N * K + speak * N]);
          }
        }
      }
    }

    // Initialize spectrum buffers
    for (int i = 0; i <= st->frame_size; i++)
      st->Rf[i] = st->Yf[i] = st->Xf[i] = 0;

    Word32 Dbf = 0;
    Word32 See = 0;

#ifdef TWO_PATH
    // Compute background filter output
    for (int chan = 0; chan < C; chan++) {
      spectralMulAccum(st->X, st->W + chan * N * K * M, st->Y + chan * N, N,
                       M * K);
      echo_ifft(st->fft_table, st->Y + chan * N, st->y + chan * N);
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
    st->Dvar1 = var1_smooth * st->Dvar1 + (.4f * Sff) * (.4f * Dbf);
    st->Dvar2 = var2_smooth * st->Dvar2 + (.15f * Sff) * (.15f * Dbf);

    int update_foreground = 0;
    if ((Sff - See) * fabsf(Sff - See) > Sff * Dbf)
      update_foreground = 1;
    else if (st->Davg1 * fabsf((float)st->Davg1) > var1_update * st->Dvar1)
      update_foreground = 1;
    else if (st->Davg2 * fabsf((float)st->Davg2) > var2_update * st->Dvar2)
      update_foreground = 1;

    if (update_foreground) {
      st->Davg1 = st->Davg2 = 0;
      st->Dvar1 = st->Dvar2 = 0.0f;
      std::memcpy(st->foreground, st->W, N * M * C * K * sizeof(Word16));
      for (int chan = 0; chan < C; chan++)
        for (int i = 0; i < st->frame_size; i++)
          st->e[chan * N + i + st->frame_size] =
              st->window[i + st->frame_size] *
                  st->e[chan * N + i + st->frame_size] +
              st->window[i] * st->y[chan * N + i + st->frame_size];
    } else {
      int reset_background = 0;
      if ((-(Sff - See)) * fabsf((float)(Sff - See)) >
          var_backtrack * (Sff * Dbf))
        reset_background = 1;
      if ((-st->Davg1) * fabsf((float)st->Davg1) > var_backtrack * st->Dvar1)
        reset_background = 1;
      if ((-st->Davg2) * fabsf((float)st->Davg2) > var_backtrack * st->Dvar2)
        reset_background = 1;

      if (reset_background) {
        std::memcpy(st->W, st->foreground, N * M * C * K * sizeof(Word32));
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
        st->Dvar1 = st->Dvar2 = 0.0f;
      }
    }
#endif

    Word32 Sey = 0, Syy = 0, Sdd = 0;
    for (int chan = 0; chan < C; chan++) {
      // Compute output with de-emphasis
      for (int i = 0; i < st->frame_size; i++) {
        Word32 tmp_out;
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
      echo_fft(st->fft_table, st->e + chan * N, st->E + chan * N);

      for (int i = 0; i < st->frame_size; i++) st->y[i + chan * N] = 0;
      echo_fft(st->fft_table, st->y + chan * N, st->Y + chan * N);

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

    if (See < N * 100.0f) See = N * 100.0f;

    for (int speak = 0; speak < K; speak++) {
      Sxx += mdfInnerProd(st->x + speak * N + st->frame_size,
                          st->x + speak * N + st->frame_size, st->frame_size);
      powerSpectrumAccum(st->X + speak * N, st->Xf, N);
    }

    // Smooth far-end energy
    for (int j = 0; j <= st->frame_size; j++)
      st->power[j] = ss_1 * st->power[j] + 1 + ss * st->Xf[j];

    // Compute filtered spectra and correlations
    Num Pey = 0.0f, Pyy = 0.0f;
    for (int j = st->frame_size; j >= 0; j--) {
      Num Eh = st->Rf[j] - st->Eh[j];
      Num Yh = st->Yf[j] - st->Yh[j];
      Pey = Pey + Eh * Yh;
      Pyy = Pyy + Yh * Yh;
      st->Eh[j] =
          (1 - st->spec_average) * st->Eh[j] + st->spec_average * st->Rf[j];
      st->Yh[j] =
          (1 - st->spec_average) * st->Yh[j] + st->spec_average * st->Yf[j];
    }

    Pyy = spxSqrt(Pyy);
    // Pyy is 0 on the very first frames (no echo spectrum has built up yet),
    // which would make Pey/Pyy evaluate to 0/0 = NaN and permanently poison
    // st->Pey / leak_estimate (a NaN comparison is always false, so the
    // clamps below can never recover from it). Floor it at 1.0f, same
    // as the clamp already applied to st->Pyy a few lines down.
    if (Pyy < 1.0f) Pyy = 1.0f;
    Pey = Pey / Pyy;

    // Compute correlation update rate
    Word32 tmp32 = st->beta0 * Syy;
    if (tmp32 > st->beta_max * See) tmp32 = st->beta_max * See;
    Num alpha = tmp32 / See;
    Num alpha_1 = 1.0f - alpha;

    st->Pey = alpha_1 * st->Pey + alpha * Pey;
    st->Pyy = alpha_1 * st->Pyy + alpha * Pyy;
    if (st->Pyy < 1.0f) st->Pyy = 1.0f;
    if (st->Pey < min_leak * st->Pyy) st->Pey = min_leak * st->Pyy;
    if (st->Pey > st->Pyy) st->Pey = st->Pyy;

    st->leak_estimate = st->Pey / st->Pyy;
    if (st->leak_estimate > 16383)
      st->leak_estimate = 32767;
    else
      st->leak_estimate = st->leak_estimate * 2;

    // Compute RER
    Word16 RER;
    RER = (.0001f * Sxx + 3.f * st->leak_estimate * Syy) / See;
    if (RER < Sey * Sey / (1 + See * Syy)) RER = Sey * Sey / (1 + See * Syy);
    if (RER > .5f) RER = .5f;

    if (!st->adapted && st->sum_adapt > M &&
        st->leak_estimate * Syy > .03f * Syy) {
      st->adapted = 1;
    }

    if (st->adapted) {
      for (int i = 0; i <= st->frame_size; i++) {
        Word32 r = st->leak_estimate * st->Yf[i];
        Word32 e = st->Rf[i] + 1;
        if (r > .5f * e) r = .5f * e;
        r = .7f * r + .3f * (RER * e);
        st->power_1[i] = r / (e * (st->power[i] + 10));
      }
    } else {
      Word16 adapt_rate = 0;
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
                 st->frame_size * sizeof(Word16));
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
 * @param size FFT size (number of points); this is the MDF window size
 *             (2x the frame size), independent of whatever length the
 *             caller's AudioFFTBase happens to be configured with.
 * @param driver Pointer to the raw FFT driver (e.g. obtained via
 *               AudioFFTBase::driver())
 * @return Opaque pointer to FFT state, or nullptr on failure
 */
inline void* echo_fft_init(int size, FFTDriver* driver) {
  if (!driver) {
    return nullptr;
  }

  // Re-initialize the raw driver at the window size the MDF algorithm
  // actually needs (independent of the caller's AudioFFTBase::config()).
  if (!driver->begin(size)) {
    return nullptr;
  }
  return new fft_state(size, driver);
}

/**
 * @brief Destroy FFT state and release resources
 * @param table Opaque pointer to FFT state returned by echo_fft_init
 */
inline void echo_fft_destroy(void* table) {
  if (table) {
    auto* st = static_cast<fft_state*>(table);
    st->driver->end();
    delete st;
  }
}

/**
 * @brief Perform forward FFT
 * @tparam T Element type (SampleType::word16_t -- float or PseudoFloat)
 * @param table Opaque pointer to FFT state
 * @param in Input time-domain signal (size N)
 * @param out Output frequency-domain signal in packed format (size N)
 *            Format: [DC, real1, imag1, real2, imag2, ..., Nyquist]
 */
template <typename T>
inline void echo_fft(void* table, T* in, T* out) {
  auto* st = static_cast<fft_state*>(table);
  if (!st || !st->driver) return;

  // Set input values
  for (int i = 0; i < st->N; i++) {
    st->driver->setValue(i, (float)in[i] / (float)st->N);
  }

  // Perform FFT
  st->driver->fft();

  // Get output in packed format: out[0]=real[0], out[1]=real[1],
  // out[2]=img[1],
  // ...
  // Note: getValue() only reflects the frequency domain after rfft()
  // (inverse); right after fft() (forward), it is still the raw input, so
  // DC/Nyquist must come from getBin() too (its imaginary part is always 0
  // for those two bins).
  FFTBin dc_bin;
  st->driver->getBin(0, dc_bin);
  out[0] = dc_bin.real;  // DC component
  for (int i = 1; i < st->N - 1; i += 2) {
    int bin = (i + 1) / 2;
    FFTBin fft_bin;
    st->driver->getBin(bin, fft_bin);
    out[i] = fft_bin.real;
    out[i + 1] = fft_bin.img;
  }
  FFTBin nyquist_bin;
  st->driver->getBin(st->N / 2, nyquist_bin);
  out[st->N - 1] = nyquist_bin.real;  // Nyquist
}

/**
 * @brief Perform inverse FFT
 * @tparam T Element type (SampleType::word16_t -- float or PseudoFloat)
 * @param table Opaque pointer to FFT state
 * @param in Input frequency-domain signal in packed format (size N)
 *           Format: [DC, real1, imag1, real2, imag2, ..., Nyquist]
 * @param out Output time-domain signal (size N)
 */
template <typename T>
inline void echo_ifft(void* table, T* in, T* out) {
  auto* st = static_cast<fft_state*>(table);
  if (!st || !st->driver || !st->driver->isReverseFFT()) return;

  // Set bins from packed format
  st->driver->setBin(0, (float)in[0], 0);
  for (int i = 1; i < st->N - 1; i += 2) {
    int bin = (i + 1) / 2;
    st->driver->setBin(bin, (float)in[i], (float)in[i + 1]);
  }
  st->driver->setBin(st->N / 2, (float)in[st->N - 1], 0);

  // Perform inverse FFT
  st->driver->rfft();

  // Get output
  for (int i = 0; i < st->N; i++) {
    out[i] = st->driver->getValue(i);
  }
}

}  // namespace audio_tools
