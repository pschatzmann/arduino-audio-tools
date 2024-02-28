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

#ifndef _HEAVY_MATH_H_
#define _HEAVY_MATH_H_

#include "HvUtils.h"

// https://software.intel.com/sites/landingpage/IntrinsicsGuide/
// https://gcc.gnu.org/onlinedocs/gcc-4.8.1/gcc/ARM-NEON-Intrinsics.html
// http://codesuppository.blogspot.co.uk/2015/02/sse2neonh-porting-guide-and-header-file.html

static inline void __hv_zero_f(hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_setzero_ps();
#elif HV_SIMD_SSE
  *bOut = _mm_setzero_ps();
#elif HV_SIMD_NEON
  *bOut = vdupq_n_f32(0.0f);
#else // HV_SIMD_NONE
  *bOut = 0.0f;
#endif
}

static inline void __hv_zero_i(hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_setzero_si256();
#elif HV_SIMD_SSE
  *bOut = _mm_setzero_si128();
#elif HV_SIMD_NEON
  *bOut = vdupq_n_s32(0);
#else // HV_SIMD_NONE
  *bOut = 0;
#endif
}

static inline void __hv_load_f(float *bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_load_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_load_ps(bIn);
#elif HV_SIMD_NEON
  *bOut = vld1q_f32(bIn);
#else // HV_SIMD_NONE
  *bOut = *bIn;
#endif
}

static inline void __hv_store_f(float *bOut, hv_bInf_t bIn) {
#if HV_SIMD_AVX
  _mm256_store_ps(bOut, bIn);
#elif HV_SIMD_SSE
  _mm_store_ps(bOut, bIn);
#elif HV_SIMD_NEON
  vst1q_f32(bOut, bIn);
#else // HV_SIMD_NONE
  *bOut = bIn;
#endif
}

static inline void __hv_log2_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_log2_f() not implemented
#elif HV_SIMD_SSE
  // https://en.wikipedia.org/wiki/Fast_inverse_square_root
  __m128i a = _mm_castps_si128(bIn);
  __m128i b = _mm_srli_epi32(a, 23);
  __m128i c = _mm_sub_epi32(b, _mm_set1_epi32(127)); // exponent (int)
  __m128 d = _mm_cvtepi32_ps(c); // exponent (float)
  __m128i e = _mm_or_si128(_mm_andnot_si128(_mm_set1_epi32(0xFF800000), a), _mm_set1_epi32(0x3F800000));
  __m128 f = _mm_castsi128_ps(e); // 1+m (float)
  __m128 g = _mm_add_ps(d, f); // e + 1 + m
  __m128 h = _mm_add_ps(g, _mm_set1_ps(-0.9569643f)); // e + 1 + m + (sigma-1)
  *bOut = h;
#elif HV_SIMD_NEON
  int32x4_t a = vreinterpretq_s32_f32(bIn);
  int32x4_t b = vshrq_n_s32(a, 23);
  int32x4_t c = vsubq_s32(b, vdupq_n_s32(127));
  float32x4_t d = vcvtq_f32_s32(c);
  int32x4_t e = vorrq_s32(vbicq_s32(a, vdupq_n_s32(0xFF800000)), vdupq_n_s32(0x3F800000));
  float32x4_t f = vreinterpretq_f32_s32(e);
  float32x4_t g = vaddq_f32(d, f);
  float32x4_t h = vaddq_f32(g, vdupq_n_f32(-0.9569643f));
  *bOut = h;
#else // HV_SIMD_NONE
  *bOut = 1.442695040888963f * hv_log_f(bIn);
#endif
}

// NOTE(mhroth): this is a pretty ghetto implementation
static inline void __hv_cos_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_set_ps(
      hv_cos_f(bIn[7]), hv_cos_f(bIn[6]), hv_cos_f(bIn[5]), hv_cos_f(bIn[4]),
      hv_cos_f(bIn[3]), hv_cos_f(bIn[2]), hv_cos_f(bIn[1]), hv_cos_f(bIn[0]));
#elif HV_SIMD_SSE
  const float *const b = (float *) &bIn;
  *bOut = _mm_set_ps(hv_cos_f(b[3]), hv_cos_f(b[2]), hv_cos_f(b[1]), hv_cos_f(b[0]));
#elif HV_SIMD_NEON
  *bOut = (float32x4_t) {hv_cos_f(bIn[0]), hv_cos_f(bIn[1]), hv_cos_f(bIn[2]), hv_cos_f(bIn[3])};
#else // HV_SIMD_NONE
  *bOut = hv_cos_f(bIn);
#endif
}

static inline void __hv_acos_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_acos_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_acos_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_acos_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_acos_f(bIn);
#endif
}

static inline void __hv_cosh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_cosh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_cosh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_cosh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_cosh_f(bIn);
#endif
}

static inline void __hv_acosh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_acosh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_acosh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_acosh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_acosh_f(bIn);
#endif
}

static inline void __hv_sin_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_sin_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_sin_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_sin_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_sin_f(bIn);
#endif
}

static inline void __hv_asin_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_asin_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_asin_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_asin_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_asin_f(bIn);
#endif
}

static inline void __hv_sinh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_sinh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_sinh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_sinh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_sinh_f(bIn);
#endif
}

static inline void __hv_asinh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_asinh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_asinh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_asinh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_asinh_f(bIn);
#endif
}

static inline void __hv_tan_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_tan_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_tan_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_tan_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_tan_f(bIn);
#endif
}

static inline void __hv_atan_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_atan_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_atan_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_atan_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_atan_f(bIn);
#endif
}

static inline void __hv_atan2_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_atan2_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_atan2_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_atan2_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_atan2_f(bIn0, bIn1);
#endif
}

static inline void __hv_tanh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_tanh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_tanh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_tanh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_tanh_f(bIn);
#endif
}

static inline void __hv_atanh_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  hv_assert(0); // __hv_atanh_f() not implemented
#elif HV_SIMD_SSE
  hv_assert(0); // __hv_atanh_f() not implemented
#elif HV_SIMD_NEON
  hv_assert(0); // __hv_atanh_f() not implemented
#else // HV_SIMD_NONE
  *bOut = hv_atanh_f(bIn);
#endif
}

static inline void __hv_sqrt_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_sqrt_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_sqrt_ps(bIn);
#elif HV_SIMD_NEON
  const float32x4_t y = vrsqrteq_f32(bIn);
  *bOut = vmulq_f32(bIn, vmulq_f32(vrsqrtsq_f32(vmulq_f32(bIn, y), y), y)); // numerical results may be inexact
#else // HV_SIMD_NONE
  *bOut = hv_sqrt_f(bIn);
#endif
}

static inline void __hv_rsqrt_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_rsqrt_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_rsqrt_ps(bIn);
#elif HV_SIMD_NEON
  const float32x4_t y = vrsqrteq_f32(bIn);
  *bOut = vmulq_f32(vrsqrtsq_f32(vmulq_f32(bIn, y), y), y); // numerical results may be inexact
#else // HV_SIMD_NONE
  *bOut = 1.0f/hv_sqrt_f(bIn);
#endif
}

static inline void __hv_abs_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_andnot_ps(_mm_set1_ps(-0.0f), bIn); // == 1 << 31
#elif HV_SIMD_NEON
  *bOut = vabsq_f32(bIn);
#else // HV_SIMD_NONE
  *bOut = hv_abs_f(bIn);
#endif
}

static inline void __hv_neg_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_xor_ps(bIn, _mm256_set1_ps(-0.0f));
#elif HV_SIMD_SSE
  *bOut = _mm_xor_ps(bIn, _mm_set1_ps(-0.0f));
#elif HV_SIMD_NEON
  *bOut = vnegq_f32(bIn);
#else // HV_SIMD_NONE
  *bOut = bIn * -1.0f;
#endif
}

static inline void __hv_exp_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  float *const b = (float *) hv_alloca(HV_N_SIMD*sizeof(float));
  _mm256_store_ps(b, bIn);
  *bOut = _mm256_set_ps(
      hv_exp_f(b[7]), hv_exp_f(b[6]), hv_exp_f(b[5]), hv_exp_f(b[4]),
      hv_exp_f(b[3]), hv_exp_f(b[2]), hv_exp_f(b[1]), hv_exp_f(b[0]));
#elif HV_SIMD_SSE
  float *const b = (float *) hv_alloca(HV_N_SIMD*sizeof(float));
  _mm_store_ps(b, bIn);
  *bOut = _mm_set_ps(hv_exp_f(b[3]), hv_exp_f(b[2]), hv_exp_f(b[1]), hv_exp_f(b[0]));
#elif HV_SIMD_NEON
  *bOut = (float32x4_t) {
    hv_exp_f(bIn[0]),
    hv_exp_f(bIn[1]),
    hv_exp_f(bIn[2]),
    hv_exp_f(bIn[3])};
#else // HV_SIMD_NONE
  *bOut = hv_exp_f(bIn);
#endif
}

static inline void __hv_ceil_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_ceil_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_ceil_ps(bIn);
#elif HV_SIMD_NEON
#if __ARM_ARCH >= 8
  *bOut = vrndpq_f32(bIn);
#else
  // A slow NEON implementation of __hv_ceil_f() is being used because
  // the necessary intrinsic cannot be found. It is only available in ARMv8.
  *bOut = (float32x4_t) {hv_ceil_f(bIn[0]), hv_ceil_f(bIn[1]), hv_ceil_f(bIn[2]), hv_ceil_f(bIn[3])};
#endif // vrndpq_f32
#else // HV_SIMD_NONE
  *bOut = hv_ceil_f(bIn);
#endif
}

static inline void __hv_floor_f(hv_bInf_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_floor_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_floor_ps(bIn);
#elif HV_SIMD_NEON
#if __ARM_ARCH >= 8
  *bOut = vrndmq_f32(bIn);
#else
  // A slow implementation of __hv_floor_f() is being used because
  // the necessary intrinsic cannot be found. It is only available from ARMv8.
  *bOut = (float32x4_t) {hv_floor_f(bIn[0]), hv_floor_f(bIn[1]), hv_floor_f(bIn[2]), hv_floor_f(bIn[3])};
#endif // vrndmq_f32
#else // HV_SIMD_NONE
  *bOut = hv_floor_f(bIn);
#endif
}

// __add~f
static inline void __hv_add_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_add_ps(bIn0, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_add_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vaddq_f32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = bIn0 + bIn1;
#endif
}

// __add~i
static inline void __hv_add_i(hv_bIni_t bIn0, hv_bIni_t bIn1, hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  __m128i x = _mm_add_epi32(_mm256_castsi256_si128(bIn0), _mm256_castsi256_si128(bIn1));
  __m128i y = _mm_add_epi32(_mm256_extractf128_si256(bIn0, 1), _mm256_extractf128_si256(bIn1, 1));
  *bOut = _mm256_insertf128_si256(_mm256_castsi128_si256(x), y, 1);
#elif HV_SIMD_SSE
  *bOut = _mm_add_epi32(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vaddq_s32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = bIn0 + bIn1;
#endif
}

// __sub~f
static inline void __hv_sub_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_sub_ps(bIn0, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_sub_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vsubq_f32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = bIn0 - bIn1;
#endif
}

// __mul~f
static inline void __hv_mul_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_mul_ps(bIn0, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_mul_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vmulq_f32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = bIn0 * bIn1;
#endif
}

// __*~i
static inline void __hv_mul_i(hv_bIni_t bIn0, hv_bIni_t bIn1, hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  __m128i x = _mm_mullo_epi32(_mm256_castsi256_si128(bIn0), _mm256_castsi256_si128(bIn1));
  __m128i y = _mm_mullo_epi32(_mm256_extractf128_si256(bIn0, 1), _mm256_extractf128_si256(bIn1, 1));
  *bOut = _mm256_insertf128_si256(_mm256_castsi128_si256(x), y, 1);
#elif HV_SIMD_SSE
  *bOut = _mm_mullo_epi32(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vmulq_s32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = bIn0 * bIn1;
#endif
}

// __cast~if
static inline void __hv_cast_if(hv_bIni_t bIn, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cvtepi32_ps(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_cvtepi32_ps(bIn);
#elif HV_SIMD_NEON
  *bOut = vcvtq_f32_s32(bIn);
#else // HV_SIMD_NONE
  *bOut = (float) bIn;
#endif
}

// __cast~fi
static inline void __hv_cast_fi(hv_bInf_t bIn, hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cvtps_epi32(bIn);
#elif HV_SIMD_SSE
  *bOut = _mm_cvtps_epi32(bIn);
#elif HV_SIMD_NEON
  *bOut = vcvtq_s32_f32(bIn);
#else // HV_SIMD_NONE
  *bOut = (int) bIn;
#endif
}

static inline void __hv_div_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  __m256 a = _mm256_cmp_ps(bIn1, _mm256_setzero_ps(), _CMP_EQ_OQ);
  __m256 b = _mm256_div_ps(bIn0, bIn1);
  *bOut = _mm256_andnot_ps(a, b);
#elif HV_SIMD_SSE
  __m128 a = _mm_cmpeq_ps(bIn1, _mm_setzero_ps());
  __m128 b = _mm_div_ps(bIn0, bIn1);
  *bOut = _mm_andnot_ps(a, b);
#elif HV_SIMD_NEON
  uint32x4_t a = vceqq_f32(bIn1, vdupq_n_f32(0.0f));
  float32x4_t b = vmulq_f32(bIn0, vrecpeq_f32(bIn1)); // NOTE(mhroth): numerical results may be inexact
  *bOut = vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(b), a));
#else // HV_SIMD_NONE
  *bOut = (bIn1 != 0.0f) ? (bIn0 / bIn1) : 0.0f;
#endif
}

static inline void __hv_min_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_min_ps(bIn0, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_min_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vminq_f32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = hv_min_f(bIn0, bIn1);
#endif
}

static inline void __hv_min_i(hv_bIni_t bIn0, hv_bIni_t bIn1, hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  __m128i x = _mm_min_epi32(_mm256_castsi256_si128(bIn0), _mm256_castsi256_si128(bIn1));
  __m128i y = _mm_min_epi32(_mm256_extractf128_si256(bIn0, 1), _mm256_extractf128_si256(bIn1, 1));
  *bOut = _mm256_insertf128_si256(_mm256_castsi128_si256(x), y, 1);
#elif HV_SIMD_SSE
  *bOut = _mm_min_epi32(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vminq_s32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = hv_min_i(bIn0, bIn1);
#endif
}

static inline void __hv_max_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_max_ps(bIn0, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_max_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vmaxq_f32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = hv_max_f(bIn0, bIn1);
#endif
}

static inline void __hv_max_i(hv_bIni_t bIn0, hv_bIni_t bIn1, hv_bOuti_t bOut) {
#if HV_SIMD_AVX
  __m128i x = _mm_max_epi32(_mm256_castsi256_si128(bIn0), _mm256_castsi256_si128(bIn1));
  __m128i y = _mm_max_epi32(_mm256_extractf128_si256(bIn0, 1), _mm256_extractf128_si256(bIn1, 1));
  *bOut = _mm256_insertf128_si256(_mm256_castsi128_si256(x), y, 1);
#elif HV_SIMD_SSE
  *bOut = _mm_max_epi32(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vmaxq_s32(bIn0, bIn1);
#else // HV_SIMD_NONE
  *bOut = hv_max_i(bIn0, bIn1);
#endif
}

static inline void __hv_pow_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  float *b = (float *) hv_alloca(16*sizeof(float));
  _mm256_store_ps(b, bIn0);
  _mm256_store_ps(b+8, bIn1);
  *bOut = _mm256_set_ps(
      hv_pow_f(b[7], b[15]),
      hv_pow_f(b[6], b[14]),
      hv_pow_f(b[5], b[13]),
      hv_pow_f(b[4], b[12]),
      hv_pow_f(b[3], b[11]),
      hv_pow_f(b[2], b[10]),
      hv_pow_f(b[1], b[9]),
      hv_pow_f(b[0], b[8]));
#elif HV_SIMD_SSE
  float *b = (float *) hv_alloca(8*sizeof(float));
  _mm_store_ps(b, bIn0);
  _mm_store_ps(b+4, bIn1);
  *bOut = _mm_set_ps(
      hv_pow_f(b[3], b[7]),
      hv_pow_f(b[2], b[6]),
      hv_pow_f(b[1], b[5]),
      hv_pow_f(b[0], b[4]));
#elif HV_SIMD_NEON
  *bOut = (float32x4_t) {
      hv_pow_f(bIn0[0], bIn1[0]),
      hv_pow_f(bIn0[1], bIn1[1]),
      hv_pow_f(bIn0[2], bIn1[2]),
      hv_pow_f(bIn0[3], bIn1[3])};
#else // HV_SIMD_NONE
  *bOut = hv_pow_f(bIn0, bIn1);
#endif
}

static inline void __hv_gt_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_GT_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmpgt_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vcgtq_f32(bIn0, bIn1));
#else // HV_SIMD_NONE
  *bOut = (bIn0 > bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_gte_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_GE_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmpge_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vcgeq_f32(bIn0, bIn1));
#else // HV_SIMD_NONE
  *bOut = (bIn0 >= bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_lt_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_LT_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmplt_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vcltq_f32(bIn0, bIn1));
#else // HV_SIMD_NONE
  *bOut = (bIn0 < bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_lte_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_LE_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmple_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vcleq_f32(bIn0, bIn1));
#else // HV_SIMD_NONE
  *bOut = (bIn0 <= bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_eq_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_EQ_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmpeq_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vceqq_f32(bIn0, bIn1));
#else // HV_SIMD_NONE
  *bOut = (bIn0 == bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_neq_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_cmp_ps(bIn0, bIn1, _CMP_NEQ_OQ);
#elif HV_SIMD_SSE
  *bOut = _mm_cmpneq_ps(bIn0, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vmvnq_u32(vceqq_f32(bIn0, bIn1)));
#else // HV_SIMD_NONE
  *bOut = (bIn0 != bIn1) ? 1.0f : 0.0f;
#endif
}

static inline void __hv_or_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_or_ps(bIn1, bIn0);
#elif HV_SIMD_SSE
  *bOut = _mm_or_ps(bIn1, bIn0);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(bIn1), vreinterpretq_u32_f32(bIn0)));
#else // HV_SIMD_NONE
  if (bIn0 == 0.0f && bIn1 == 0.0f) *bOut = 0.0f;
  else if (bIn0 == 0.0f) *bOut = bIn1;
  else if (bIn1 == 0.0f) *bOut = bIn0;
  else hv_assert(0);
#endif
}

static inline void __hv_and_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_and_ps(bIn1, bIn0);
#elif HV_SIMD_SSE
  *bOut = _mm_and_ps(bIn1, bIn0);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(bIn1), vreinterpretq_u32_f32(bIn0)));
#else // HV_SIMD_NONE
  if (bIn0 == 0.0f || bIn1 == 0.0f) *bOut = 0.0f;
  else if (bIn0 == 1.0f) *bOut = bIn1;
  else if (bIn1 == 1.0f) *bOut = bIn0;
  else hv_assert(0);
#endif
}

static inline void __hv_andnot_f(hv_bInf_t bIn0_mask, hv_bInf_t bIn1, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
  *bOut = _mm256_andnot_ps(bIn0_mask, bIn1);
#elif HV_SIMD_SSE
  *bOut = _mm_andnot_ps(bIn0_mask, bIn1);
#elif HV_SIMD_NEON
  *bOut = vreinterpretq_f32_s32(vbicq_s32(vreinterpretq_s32_f32(bIn1), vreinterpretq_s32_f32(bIn0_mask)));
#else // HV_SIMD_NONE
  *bOut = (bIn0_mask == 0.0f) ? bIn1 : 0.0f;
#endif
}

// bOut = (bIn0 * bIn1) + bIn2
static inline void __hv_fma_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bInf_t bIn2, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
#if HV_SIMD_FMA
  *bOut = _mm256_fmadd_ps(bIn0, bIn1, bIn2);
#else
  *bOut = _mm256_add_ps(_mm256_mul_ps(bIn0, bIn1), bIn2);
#endif // HV_SIMD_FMA
#elif HV_SIMD_SSE
#if HV_SIMD_FMA
  *bOut = _mm_fmadd_ps(bIn0, bIn1, bIn2);
#else
  *bOut = _mm_add_ps(_mm_mul_ps(bIn0, bIn1), bIn2);
#endif // HV_SIMD_FMA
#elif HV_SIMD_NEON
#if __ARM_ARCH >= 8
  *bOut = vfmaq_f32(bIn2, bIn0, bIn1);
#else
  // NOTE(mhroth): it turns out, fma SUUUUCKS on lesser ARM architectures
  *bOut = vaddq_f32(vmulq_f32(bIn0, bIn1), bIn2);
#endif
#else // HV_SIMD_NONE
  *bOut = hv_fma_f(bIn0, bIn1, bIn2);
#endif
}

// bOut = (bIn0 * bIn1) - bIn2
static inline void __hv_fms_f(hv_bInf_t bIn0, hv_bInf_t bIn1, hv_bInf_t bIn2, hv_bOutf_t bOut) {
#if HV_SIMD_AVX
#if HV_SIMD_FMA
  *bOut = _mm256_fmsub_ps(bIn0, bIn1, bIn2);
#else
  *bOut = _mm256_sub_ps(_mm256_mul_ps(bIn0, bIn1), bIn2);
#endif // HV_SIMD_FMA
#elif HV_SIMD_SSE
#if HV_SIMD_FMA
  *bOut = _mm_fmsub_ps(bIn0, bIn1, bIn2);
#else
  *bOut = _mm_sub_ps(_mm_mul_ps(bIn0, bIn1), bIn2);
#endif // HV_SIMD_FMA
#elif HV_SIMD_NEON
#if __ARM_ARCH >= 8
  *bOut = vfmsq_f32(bIn2, bIn0, bIn1);
#else
  // NOTE(mhroth): it turns out, fma SUUUUCKS on lesser ARM architectures
  *bOut = vsubq_f32(vmulq_f32(bIn0, bIn1), bIn2);
#endif
#else // HV_SIMD_NONE
  *bOut = (bIn0 * bIn1) - bIn2;
#endif
}

#endif // _HEAVY_MATH_H_
