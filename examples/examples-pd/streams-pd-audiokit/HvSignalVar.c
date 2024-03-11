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

#include "HvSignalVar.h"

// __var~f

static void sVarf_update(SignalVarf *o, float k, float step, bool reverse) {
#if HV_SIMD_AVX
  if (reverse) o->v = _mm256_setr_ps(k+7.0f*step, k+6.0f*step, k+5.0f*step, k+4.0f*step, k+3.0f*step, k+2.0f*step, k+step, k);
  else o->v = _mm256_set_ps(k+7.0f*step, k+6.0f*step, k+5.0f*step, k+4.0f*step, k+3.0f*step, k+2.0f*step, k+step, k);
#elif HV_SIMD_SSE
  if (reverse) o->v = _mm_setr_ps(k+3.0f*step, k+2.0f*step, k+step, k);
  else o->v = _mm_set_ps(k+3.0f*step, k+2.0f*step, k+step, k);
#elif HV_SIMD_NEON
  if (reverse) o->v = (float32x4_t) {3.0f*step+k, 2.0f*step+k, step+k, k};
  else o->v = (float32x4_t) {k, step+k, 2.0f*step+k, 3.0f*step+k};
#else // HV_SIMD_NONE
  o->v = k;
#endif
}

hv_size_t sVarf_init(SignalVarf *o, float k, float step, bool reverse) {
  sVarf_update(o, k, step, reverse);
  return 0;
}

void sVarf_onMessage(HeavyContextInterface *_c, SignalVarf *o, const HvMessage *m) {
  if (msg_isFloat(m,0)) {
    sVarf_update(o, msg_getFloat(m,0), msg_isFloat(m,1) ? msg_getFloat(m,1) : 0.0f, msg_getNumElements(m) == 3);
  }
}



// __var~i

static void sVari_update(SignalVari *o, int k, int step, bool reverse) {
#if HV_SIMD_AVX
  if (reverse) o->v = _mm256_setr_epi32(k+7*step, k+6*step, k+5*step, k+4*step, k+3*step, k+2*step, k+step, k);
  else o->v = _mm256_set_epi32(k+7*step, k+6*step, k+5*step, k+4*step, k+3*step, k+2*step, k+step, k);
#elif HV_SIMD_SSE
  if (reverse) o->v = _mm_setr_epi32(k+3*step, k+2*step, k+step, k);
  else o->v = _mm_set_epi32(k+3*step, k+2*step, k+step, k);
#elif HV_SIMD_NEON
  if (reverse) o->v = (int32x4_t) {3*step+k, 2*step+k, step+k, k};
  else o->v = (int32x4_t) {k, step+k, 2*step+k, 3*step+k};
#else // HV_SIMD_NEON
  o->v = k;
#endif
}

hv_size_t sVari_init(SignalVari *o, int k, int step, bool reverse) {
  sVari_update(o, k, step, reverse);
  return 0;
}

void sVari_onMessage(HeavyContextInterface *_c, SignalVari *o, const HvMessage *m) {
  if (msg_isFloat(m,0)) {
    sVari_update(o, (int) msg_getFloat(m,0), msg_isFloat(m,1) ? (int) msg_getFloat(m,1) : 0, msg_getNumElements(m) == 3);
  }
}
