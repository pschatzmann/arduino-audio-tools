/**
 * Copyright (c) 2014-2018 Enzien Audio Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _HEAVY_SIGNAL_PHASOR_H_
#define _HEAVY_SIGNAL_PHASOR_H_

#include "HvHeavyInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SignalPhasor {
#if HV_SIMD_AVX
  __m256 phase; // current phase
  __m256 inc;   // phase increment
#elif HV_SIMD_SSE
  __m128i phase;
  __m128i inc;
#elif HV_SIMD_NEON
  uint32x4_t phase;
  int32x4_t inc;
#else // HV_SIMD_NONE
  hv_uint32_t phase;
  hv_int32_t inc;
#endif
  union {
    float f2sc; // float to step conversion (used for __phasor~f)
    hv_int32_t s; // step value (used for __phasor_k~f)
  } step;
} SignalPhasor;

hv_size_t sPhasor_init(SignalPhasor *o, double samplerate);

hv_size_t sPhasor_k_init(SignalPhasor *o, float frequency, double samplerate);

void sPhasor_k_onMessage(HeavyContextInterface *_c, SignalPhasor *o, int letIn, const HvMessage *m);

void sPhasor_onMessage(HeavyContextInterface *_c, SignalPhasor *o, int letIn, const HvMessage *m);

static inline void __hv_phasor_f(SignalPhasor *o, hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  __m256 p = _mm256_mul_ps(bIn, _mm256_set1_ps(o->step.f2sc)); // a b c d e f g h

  __m256 z = _mm256_setzero_ps();

  // http://stackoverflow.com/questions/11906814/how-to-rotate-an-sse-avx-vector
  __m256 a = _mm256_permute_ps(p, _MM_SHUFFLE(2,1,0,3)); // d a b c h e f g
  __m256 b = _mm256_permute2f128_ps(a, a, 0x01);         // h e f g d a b c
  __m256 c = _mm256_blend_ps(a, b, 0x10);                // d a b c d e f g
  __m256 d = _mm256_blend_ps(c, z, 0x01);                // 0 a b c d e f g
  __m256 e = _mm256_add_ps(p, d); // a (a+b) (b+c) (c+d) (d+e) (e+f) (f+g) (g+h)

  __m256 f = _mm256_permute_ps(e, _MM_SHUFFLE(1,0,3,2)); // (b+c) (c+d) a (a+b) (f+g) (g+h) (d+e) (e+f)
  __m256 g = _mm256_permute2f128_ps(f, f, 0x01);         // (f+g) (g+h) (d+e) (e+f) (b+c) (c+d) a (a+b)
  __m256 h = _mm256_blend_ps(f, g, 0x33);                // (b+c) (c+d) a (a+b) (b+c) (c+d) (d+e) (e+f)
  __m256 i = _mm256_blend_ps(h, z, 0x03);                // 0 0 a (a+b) (b+c) (c+d) (d+e) (e+f)
  __m256 j = _mm256_add_ps(e, i); // a (a+b) (a+b+c) (a+b+c+d) (b+c+d+e) (c+d+e+f) (d+e+f+g) (e+f+g+h)

  __m256 k = _mm256_permute2f128_ps(j, z, 0x02);         // 0 0 0 0 a (a+b) (a+b+c) (a+b+c+d) (b+c+d+e)
  __m256 m = _mm256_add_ps(j, k); // a (a+b) (a+b+c) (a+b+c+d) (a+b+c+d+e) (a+b+c+d+e+f) (a+b+c+d+e+f+g) (a+b+c+d+e+f+g+h)

  __m256 n = _mm256_or_ps(_mm256_andnot_ps(
      _mm256_set1_ps(-INFINITY),
      _mm256_add_ps(o->phase, m)),
      _mm256_set1_ps(1.0f));

  *bOut = _mm256_sub_ps(n, _mm256_set1_ps(1.0f));

  __m256 x = _mm256_permute_ps(n, _MM_SHUFFLE(3,3,3,3));
  o->phase = _mm256_permute2f128_ps(x, x, 0x11);
#elif HV_SIMD_SSE
  __m128i p = _mm_cvtps_epi32(_mm_mul_ps(bIn, _mm_set1_ps(o->step.f2sc))); // convert frequency to step
  p = _mm_add_epi32(p, _mm_slli_si128(p, 4)); // add incremental steps to phase (prefix sum)
  p = _mm_add_epi32(p, _mm_slli_si128(p, 8)); // http://stackoverflow.com/questions/10587598/simd-prefix-sum-on-intel-cpu?rq=1
  p = _mm_add_epi32(o->phase, p);
  *bOut = _mm_sub_ps(_mm_castsi128_ps(
      _mm_or_si128(_mm_srli_epi32(p, 9),
      _mm_set_epi32(0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000))),
      _mm_set1_ps(1.0f));
  o->phase = _mm_shuffle_epi32(p, _MM_SHUFFLE(3,3,3,3));
#elif HV_SIMD_NEON
  int32x4_t p = vcvtq_s32_f32(vmulq_n_f32(bIn, o->step.f2sc));
  p = vaddq_s32(p, vextq_s32(vdupq_n_s32(0), p, 3)); // http://stackoverflow.com/questions/11259596/arm-neon-intrinsics-rotation
  p = vaddq_s32(p, vextq_s32(vdupq_n_s32(0), p, 2));
  uint32x4_t pp = vaddq_u32(o->phase, vreinterpretq_u32_s32(p));
  *bOut = vsubq_f32(vreinterpretq_f32_u32(vorrq_u32(vshrq_n_u32(pp, 9), vdupq_n_u32(0x3F800000))), vdupq_n_f32(1.0f));
  o->phase = vdupq_n_u32(pp[3]);
#else // HV_SIMD_NONE
  const hv_uint32_t p = (o->phase >> 9) | 0x3F800000;
  *bOut = *((float *) (&p)) - 1.0f;
  o->phase += ((int) (bIn * o->step.f2sc));
#endif
}

static inline void __hv_phasor_k_f(SignalPhasor *o, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_sub_ps(o->phase, _mm256_set1_ps(1.0f));
  o->phase = _mm256_or_ps(_mm256_andnot_ps(
      _mm256_set1_ps(-INFINITY),
      _mm256_add_ps(o->phase, o->inc)),
      _mm256_set1_ps(1.0f));
#elif HV_SIMD_SSE
  *bOut = _mm_sub_ps(_mm_castsi128_ps(
      _mm_or_si128(_mm_srli_epi32(o->phase, 9),
      _mm_set_epi32(0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000))),
      _mm_set1_ps(1.0f));
  o->phase = _mm_add_epi32(o->phase, o->inc);
#elif HV_SIMD_NEON
  *bOut = vsubq_f32(vreinterpretq_f32_u32(
      vorrq_u32(vshrq_n_u32(o->phase, 9),
      vdupq_n_u32(0x3F800000))),
      vdupq_n_f32(1.0f));
  o->phase = vaddq_u32(o->phase, vreinterpretq_u32_s32(o->inc));
#else // HV_SIMD_NONE
  const hv_uint32_t p = (o->phase >> 9) | 0x3F800000;
  *bOut = *((float *) (&p)) - 1.0f;
  o->phase += o->inc;
#endif
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _HEAVY_SIGNAL_PHASOR_H_
