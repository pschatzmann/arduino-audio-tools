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

#ifndef _HEAVY_SIGNAL_VAR_H_
#define _HEAVY_SIGNAL_VAR_H_

#include "HvHeavyInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

// __var~f, __varread~f, __varwrite~f

typedef struct SignalVarf {
  hv_bufferf_t v;
} SignalVarf;

hv_size_t sVarf_init(SignalVarf *o, float k, float step, bool reverse);

static inline void __hv_varread_f(SignalVarf *o, hv_bOutf_t bOut) {
  *bOut = o->v;
}

static inline void __hv_varwrite_f(SignalVarf *o, hv_bInf_t bIn) {
  o->v = bIn;
}

void sVarf_onMessage(HeavyContextInterface *_c, SignalVarf *o, const HvMessage *m);



// __var~i, __varread~i, __varwrite~i

typedef struct SignalVari {
  hv_bufferi_t v;
} SignalVari;

hv_size_t sVari_init(SignalVari *o, int k, int step, bool reverse);

static inline void __hv_varread_i(SignalVari *o, hv_bOuti_t bOut) {
  *bOut = o->v;
}

static inline void __hv_varwrite_i(SignalVari *o, hv_bIni_t bIn) {
  o->v = bIn;
}

void sVari_onMessage(HeavyContextInterface *_c, SignalVari *o, const HvMessage *m);



// __var_k~f, __var_k~i

#if HV_SIMD_AVX
#define __hv_var_k_i(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm256_set_epi32(_h,_g,_f,_e,_d,_c,_b,_a)
#define __hv_var_k_i_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm256_set_epi32(_a,_b,_c,_d,_e,_f,_g,_h)
#define __hv_var_k_f(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm256_set_ps(_h,_g,_f,_e,_d,_c,_b,_a)
#define __hv_var_k_f_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm256_set_ps(_a,_b,_c,_d,_e,_f,_g,_h)
#elif HV_SIMD_SSE
#define __hv_var_k_i(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm_set_epi32(_d,_c,_b,_a)
#define __hv_var_k_i_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm_set_epi32(_a,_b,_c,_d)
#define __hv_var_k_f(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm_set_ps(_d,_c,_b,_a)
#define __hv_var_k_f_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_mm_set_ps(_a,_b,_c,_d)
#elif HV_SIMD_NEON
#define __hv_var_k_i(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=((int32x4_t) {_a,_b,_c,_d})
#define __hv_var_k_i_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=((int32x4_t) {_d,_c,_b,_a})
#define __hv_var_k_f(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=((float32x4_t) {_a,_b,_c,_d})
#define __hv_var_k_f_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=((float32x4_t) {_d,_c,_b,_a})
#else // HV_SIMD_NONE
#define __hv_var_k_i(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_a
#define __hv_var_k_i_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_a
#define __hv_var_k_f(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_a
#define __hv_var_k_f_r(_z,_a,_b,_c,_d,_e,_f,_g,_h) *_z=_a
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _HEAVY_SIGNAL_VAR_H_
