/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell
   Written by Jean-Marc Valin and Gregory Maxwell */
/**
  @file celt.h
  @brief Contains all the functions for encoding and decoding audio
 */

/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#pragma GCC optimize ("Os")

#include "Arduino.h"
#include <stdint.h>
//#include <cstddef>
#include <assert.h>

#define OPUS_RESET_STATE             4028
#define OPUS_GET_SAMPLE_RATE_REQUEST 4029

#define LEAK_BANDS 19

typedef struct {
    int32_t valid;
    float   tonality;
    float   tonality_slope;
    float   noisiness;
    float   activity;
    float   music_prob;
    float   music_prob_min;
    float   music_prob_max;
    int32_t bandwidth;
    float   activity_probability;
    float   max_pitch_ratio;
    /* Store as Q6 char to save space. */
    uint8_t leak_boost[LEAK_BANDS];
} AnalysisInfo;

/*OPT: ec_window must be at least 32 bits, but if you have fast arithmetic on a larger type, you can speed up the
 decoder by using it here.*/

typedef struct CELTMode       CELTMode;
typedef struct CELTDecoder    CELTDecoder;

typedef struct _ec_ctx {
    uint8_t *buf;        /*Buffered input/output.*/
    uint32_t storage;    /*The size of the buffer.*/
    uint32_t end_offs;   /*The offset at which the last byte containing raw bits was read/written.*/
    uint32_t end_window; /*Bits that will be read from/written at the end.*/
    int32_t  nend_bits;  /*Number of valid bits in end_window.*/
    int32_t  nbits_total;
    uint32_t offs; /*The offset at which the next range coder byte will be read/written.*/
    uint32_t rng;  /*The number of values in the current range.*/
    uint32_t val;
    uint32_t ext;
    int32_t  rem;   /*A buffered input/output symbol, awaiting carry propagation.*/
    int32_t  error; /*Nonzero if an error occurred.*/
} ec_ctx_t;

extern ec_ctx_t s_ec;
extern const uint8_t cache_bits50[392];
extern const int16_t cache_index50[105];

typedef struct _band_ctx{
    int32_t encode;
    int32_t resynth;
    int32_t i;
    int32_t intensity;
    int32_t spread;
    int32_t tf_change;
    int32_t remaining_bits;
    uint32_t seed;
    int32_t theta_round;
    int32_t disable_inv;
    int32_t avoid_split_noise;
} band_ctx_t;

struct split_ctx{
    int32_t inv;
    int32_t imid;
    int32_t iside;
    int32_t delta;
    int32_t itheta;
    int32_t qalloc;
};

struct CELTDecoder {
    const CELTMode *mode;
    int32_t overlap;
    int32_t channels;
    int32_t stream_channels;

    int32_t start, end;
    int32_t signalling;
    int32_t disable_inv;

    uint32_t rng;
    int32_t error;
    int32_t postfilter_period;
    int32_t postfilter_period_old;
    int16_t postfilter_gain;
    int16_t postfilter_gain_old;
    int32_t postfilter_tapset;
    int32_t postfilter_tapset_old;

    int32_t preemph_memD[2];

    int32_t _decode_mem[1]; /* Size = channels*(DECODE_BUFFER_SIZE+mode->overlap) */
                            /* int16_t lpc[],  Size = channels*LPC_ORDER */
                            /* int16_t oldEBands[], Size = 2*mode->nbEBands */
                            /* int16_t oldLogE[], Size = 2*mode->nbEBands */
                            /* int16_t oldLogE2[], Size = 2*mode->nbEBands */
                            /* int16_t backgroundLogE[], Size = 2*mode->nbEBands */
};

typedef struct {
    int32_t r;
    int32_t i;
}kiss_fft_cpx;

typedef struct {
   int16_t r;
   int16_t i;
}kiss_twiddle_cpx;

#define MAXFACTORS 8

typedef struct kiss_fft_state{
    int32_t nfft;
    int16_t scale;
    int32_t scale_shift;
    int32_t shift;
    int16_t factors[2*MAXFACTORS];
    const int16_t *bitrev;
    const kiss_twiddle_cpx *twiddles;
} kiss_fft_state;

typedef struct {
   int32_t n;
   int32_t maxshift;
   const kiss_fft_state *kfft[4];
   const int16_t *  trig;
} mdct_lookup_t;



/** Mode definition (opaque)
 @brief Mode definition
 */
struct CELTMode {
    int32_t Fs;
    int32_t overlap;
    int32_t nbEBands;
    int32_t effEBands;
    int16_t preemph[4];
    int32_t maxLM;
    int32_t nbShortMdcts;
    int32_t shortMdctSize;
    int32_t nbAllocVectors;                /**< Number of lines in the matrix below */
};

extern const CELTMode m_CELTMode;
#ifndef _min
    #define _min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef _max
    #define _max(a,b) ((a)>(b)?(a):(b))
#endif

inline int32_t S_MUL(int32_t a, int16_t b){return (int64_t)b * a >> 15;}
#define C_MUL(m,a,b)  do{ (m).r = SUB32_ovflw(S_MUL((a).r,(b).r) , S_MUL((a).i,(b).i)); \
                          (m).i = ADD32_ovflw(S_MUL((a).r,(b).i) , S_MUL((a).i,(b).r)); }while(0)

#define C_MULBYSCALAR( c, s ) do{ (c).r =  S_MUL( (c).r , s ) ;  (c).i =  S_MUL( (c).i , s ) ; }while(0)

#define DIVSCALAR(x,k) (x) = S_MUL(  x, (32767-((k)>>1))/(k)+1 )

#define C_FIXDIV(c,div) do {    DIVSCALAR( (c).r , div); DIVSCALAR( (c).i  , div); }while (0)

#define C_ADD( res, a,b) do {(res).r=ADD32_ovflw((a).r,(b).r);  (res).i=ADD32_ovflw((a).i,(b).i); }while(0)

#define C_SUB( res, a,b) do {(res).r=SUB32_ovflw((a).r,(b).r);  (res).i=SUB32_ovflw((a).i,(b).i); }while(0)
#define C_ADDTO( res , a) do {(res).r = ADD32_ovflw((res).r, (a).r);  (res).i = ADD32_ovflw((res).i,(a).i); }while(0)

#define VERY_LARGE16 ((int16_t)32767)
#define Q15_ONE ((int16_t)32767)


#define EC_MINI(_a,_b)      ((_a)+(((_b)-(_a))&-((_b)<(_a))))
#define _CHAR_BIT 8
#define EC_CLZ0    ((int32_t)sizeof(uint32_t)*_CHAR_BIT)
#define EC_CLZ(_x) (__builtin_clz(_x))
#define EC_ILOG(_x) (EC_CLZ0-EC_CLZ(_x))

/** Multiply a 16-bit signed value by a 16-bit uint32_t value. The result is a 32-bit signed value */
#define MULT16_16SU(a,b) ((int32_t)(int16_t)(a)*(int32_t)(uint16_t)(b))

/** 16x32 multiplication, followed by a 16-bit shift right. Results fits in 32 bits */
inline int32_t MULT16_32_Q16(int64_t a, int64_t b){return (int32_t) (a * b) >> 16;}

/** 16x32 multiplication, followed by a 16-bit shift right (round-to-nearest). Results fits in 32 bits */
#define MULT16_32_P16(a,b) ((int32_t)PSHR((int64_t)((int16_t)(a))*(b),16))

/** 16x32 multiplication, followed by a 15-bit shift right. Results fits in 32 bits */
inline int32_t MULT16_32_Q15(int16_t a, int32_t b){return (int64_t)a * b >> 15;}

/** 32x32 multiplication, followed by a 31-bit shift right. Results fits in 32 bits */
#define MULT32_32_Q31(a,b) ((int32_t)((int64_t)(a)*(int64_t)(b) >> 31))

/** Compile-time conversion of float constant to 16-bit value */
#define QCONST16(x,bits) ((int16_t)(0.5L+(x)*(((int32_t)1)<<(bits))))

/** Compile-time conversion of float constant to 32-bit value */
#define QCONST32(x,bits) ((int32_t)(0.5L+(x)*(((int32_t)1)<<(bits))))

/** Change a 16-bit value into a 32-bit value */
#define EXTEND32(x) ((int32_t)(x))

/** Arithmetic shift-right of a 16-bit value */
#define SHR16(a,shift) ((a) >> (shift))
/** Arithmetic shift-left of a 16-bit value */
#define SHL16(a,shift) ((int16_t)((uint16_t)(a)<<(shift)))
/** Arithmetic shift-right of a 32-bit value */
#define SHR32(a,shift) ((a) >> (shift))
/** Arithmetic shift-left of a 32-bit value */
#define SHL32(a,shift) ((int32_t)((uint32_t)(a)<<(shift)))

/** 32-bit arithmetic shift right with rounding-to-nearest instead of rounding down */
inline int32_t PSHR(int32_t a, uint32_t shift){return (a + ((int32_t)1 << (shift >> 1))) >> shift;}

/** 32-bit arithmetic shift right where the argument can be negative */
#define VSHR32(a, shift) (((shift)>0) ? SHR32(a, shift) : SHL32(a, -(shift)))

#define SATURATE(x,a) (((x)>(a) ? (a) : (x)<-(a) ? -(a) : (x)))

#define SATURATE16(x) ((int16_t)((x)>32767 ? 32767 : (x)<-32768 ? -32768 : (x)))

/** Shift by a and round-to-neareast 32-bit value. Result is a 16-bit value */
#define ROUND16(x,a) ((int16_t)(PSHR((x),(a))))
/** Shift by a and round-to-neareast 32-bit value. Result is a saturated 16-bit value */
#define SROUND16(x,a) (int16_t)(SATURATE(PSHR(x,a), 32767));

/** Divide by two */
#define HALF16(x)  (SHR16(x,1))
#define HALF32(x)  (SHR32(x,1))

/** Add two 16-bit values */
#define ADD16(a,b) ((int16_t)((int16_t)(a)+(int16_t)(b)))
/** Subtract two 16-bit values */
#define SUB16(a,b) ((int16_t)(a)-(int16_t)(b))
/** Add two 32-bit values */
#define ADD32(a,b) ((int32_t)(a)+(int32_t)(b))
/** Subtract two 32-bit values */
#define SUB32(a,b) ((int32_t)(a)-(int32_t)(b))
/** Arithmetic shift-right of a 32-bit value */
#define SHR32(a,shift) ((a) >> (shift))

/** Add two 32-bit values, ignore any overflows */
#define ADD32_ovflw(a,b) ((int32_t)((uint32_t)(a)+(uint32_t)(b)))
/** Subtract two 32-bit values, ignore any overflows */
#define SUB32_ovflw(a,b) ((int32_t)((uint32_t)(a)-(uint32_t)(b)))
/* Avoid MSVC warning C4146: unary minus operator applied to uint32_t type */
/** Negate 32-bit value, ignore any overflows */
#define NEG32_ovflw(a) ((int32_t)(0-(uint32_t)(a)))

/** 16x16 multiplication where the result fits in 16 bits */
#define MULT16_16_16(a,b)     ((((int16_t)(a))*((int16_t)(b))))

/* (int32_t)(int16_t) gives TI compiler a hint that it's 16x16->32 multiply */
/** 16x16 multiplication where the result fits in 32 bits */
#define MULT16_16(a,b)     (((int32_t)(int16_t)(a))*((int32_t)(int16_t)(b)))

/** 16x16 multiply-add where the result fits in 32 bits */
#define MAC16_16(c,a,b) (ADD32((c),MULT16_16((a),(b))))
/** 16x32 multiply, followed by a 15-bit shift right and 32-bit add.
    b must fit in 31 bits.
    Result fits in 32 bits. */
#define MAC16_32_Q15(c,a,b) ADD32((c),ADD32(MULT16_16((a),(b) >> 15), MULT16_16((a),((b)&0x00007fff)) >> 15))

/** 16x32 multiplication, followed by a 16-bit shift right and 32-bit add.
    Results fits in 32 bits */
#define MAC16_32_Q16(c,a,b) ADD32((c),ADD32(MULT16_16((a),(b) >> 16), MULT16_16SU((a),((b)&0x0000ffff)) >> 16))

#define MULT16_16_Q11_32(a,b) (MULT16_16((a),(b)) >> 11)
#define MULT16_16_Q11(a,b)    (MULT16_16((a),(b)) >> 11)
#define MULT16_16_Q13(a,b)    (MULT16_16((a),(b)) >> 13)
#define MULT16_16_Q14(a,b)    (MULT16_16((a),(b)) >> 14)
#define MULT16_16_Q15(a,b)    (MULT16_16((a),(b)) >> 15)

#define MULT16_16_P13(a,b)   (ADD32(4096, MULT16_16((a),(b))) >> 13)
#define MULT16_16_P14(a,b)   (ADD32(8192, MULT16_16((a),(b))) >> 14)
#define MULT16_16_P15(a,b)   (ADD32(16384,MULT16_16((a),(b))) >> 15)

/** Divide a 32-bit value by a 16-bit value. Result fits in 16 bits */
#define DIV32_16(a,b) ((int16_t)(((int32_t)(a))/((int16_t)(b))))

/** Divide a 32-bit value by a 32-bit value. Result fits in 32 bits */
#define DIV32(a,b) (((int32_t)(a))/((int32_t)(b)))
int32_t celt_rcp(int32_t x);
#define celt_div(a,b) MULT32_32_Q31((int32_t)(a),celt_rcp(b))
#define MAX_PERIOD 1024
#define OPUS_MOVE(dst, src, n) (memmove((dst), (src), (n)*sizeof(*(dst)) + 0*((dst)-(src)) ))
#define ALLOC_STEPS 6

/* Multiplies two 16-bit fractional values. Bit-exactness of this macro is important */
#define FRAC_MUL16(a,b) ((16384+((int32_t)(int16_t)(a)*(int16_t)(b)))>>15)

#define QTHETA_OFFSET 4
#define QTHETA_OFFSET_TWOPHASE 16
#define MAX_FINE_BITS 8
#define MAX_PSEUDO 40
#define LOG_MAX_PSEUDO 6
#define ALLOC_NONE 1
#define Q15ONE 32767

/* Prototypes and inlines*/

inline int16_t SAT16(int32_t x) {
    if(x > INT16_MAX) return INT16_MAX;
    if(x < INT16_MIN) return INT16_MIN;
    return (int16_t)x;
}

inline int32_t celt_sudiv(int32_t n, int32_t d) {
   assert(d>0); return n/d;
}

inline int16_t sig2word16(int32_t x){
   x = PSHR(x, 12);
   x = _max(x, -32768);
   x = _min(x, 32767);
   return (int16_t)(x);
}

inline int32_t ec_tell(){
  return s_ec.nbits_total-EC_ILOG(s_ec.rng);
}

/* Atan approximation using a 4th order polynomial. Input is in Q15 format and normalized by pi/4. Output is in
   Q15 format */
inline int16_t celt_atan01(int16_t x) {
    return MULT16_16_P15(
        x, ADD32(32767, MULT16_16_P15(x, ADD32(-21, MULT16_16_P15(x, ADD32(-11943, MULT16_16_P15(4936, x)))))));
}

/* atan2() approximation valid for positive input values */
inline int16_t celt_atan2p(int16_t y, int16_t x) {
    if(y < x) {
        int32_t arg;
        arg = celt_div(SHL32(EXTEND32(y), 15), x);
        if(arg >= 32767) arg = 32767;
        return SHR16(celt_atan01((int16_t)(arg)), 1);
    } else {
        int32_t arg;
        arg = celt_div(SHL32(EXTEND32(x), 15), y);
        if(arg >= 32767) arg = 32767;
        return 25736 - SHR16(celt_atan01((int16_t)(arg)), 1);
    }
}

inline int32_t celt_maxabs16(const int16_t *x, int32_t len) {
    int32_t i;
    int16_t maxval = 0;
    int16_t minval = 0;
    for(i = 0; i < len; i++) {
        maxval = _max(maxval, x[i]);
        minval = _min(minval, x[i]);
    }
    return _max(EXTEND32(maxval), -EXTEND32(minval));
}

inline int32_t celt_maxabs32(const int32_t *x, int32_t len) {
    int32_t i;
    int32_t maxval = 0;
    int32_t minval = 0;
    for(i = 0; i < len; i++) {
        maxval = _max(maxval, x[i]);
        minval = _min(minval, x[i]);
    }
    return _max(maxval, -minval);
}

/** Integer log in base2. Undefined for zero and negative numbers */
inline int16_t celt_ilog2(uint32_t x) {
    assert(x > 0);
    return EC_ILOG(x) - 1;
}

/** Integer log in base2. Defined for zero, but not for negative numbers */
inline int16_t celt_zlog2(uint32_t x) { return x <= 0 ? 0 : celt_ilog2(x); }

/** Base-2 logarithm approximation (log2(x)). (Q14 input, Q10 output) */
inline int16_t celt_log2(int32_t x) {
    int32_t i;
    int16_t n, frac, var1;
    /* -0.41509302963303146, 0.9609890551383969, -0.31836011537636605, 0.15530808010959576, -0.08556153059057618 */
    static const int16_t C[5] = {-6801 + (1 << 3), 15746, -5217, 2545, -1401};
    if(x == 0) return -32767;
    i = celt_ilog2(x);
    n = VSHR32(x, i - 15) - 32768 - 16384;
    var1 = MULT16_16_Q15(n, ADD16(C[3], MULT16_16_Q15(n, C[4])));
    frac = ADD16(C[0], MULT16_16_Q15(n, ADD16(C[1], MULT16_16_Q15(n, ADD16(C[2], var1)))));
    return SHL16(i - 13, 10) + SHR16(frac, 14 - 10);
}

inline int32_t celt_exp2_frac(int16_t x) {
    int16_t frac = SHL16(x, 4);
    int16_t var1 = ADD16(14819, MULT16_16_Q15(10204, frac));
    return ADD16(16383, MULT16_16_Q15(frac, ADD16(22804, MULT16_16_Q15(frac, var1))));
}
/** Base-2 exponential approximation (2^x). (Q10 input, Q16 output) */
inline int32_t celt_exp2(int16_t x) {
    int32_t integer;
    int16_t frac;
    integer = SHR16(x, 10);
    if (integer > 14)
        return 0x7f000000;
    else if (integer < -15)
        return 0;
    frac = celt_exp2_frac(x - SHL16(integer, 10));
    return VSHR32(EXTEND32(frac), -integer - 2);
}

inline void dual_inner_prod(const int16_t *x, const int16_t *y01, const int16_t *y02, int32_t N, int32_t *xy1,
                                   int32_t *xy2) {
    int32_t i;
    int32_t xy01 = 0;
    int32_t xy02 = 0;
    for(i = 0; i < N; i++) {
        xy01 = MAC16_16(xy01, x[i], y01[i]);
        xy02 = MAC16_16(xy02, x[i], y02[i]);
    }
    *xy1 = xy01;
    *xy2 = xy02;
}

inline uint32_t celt_inner_prod(const int16_t *x, const int16_t *y, int32_t N) {
    int32_t i;
    uint32_t xy = 0;
    for (i = 0; i < N; i++) xy = (int32_t)x[i] * (int32_t)y[i] + xy;
    return xy;
}

inline int32_t get_pulses(int32_t i){
   return i<8 ? i : (8 + (i&7)) << ((i>>3)-1);
}

inline int32_t bits2pulses(int32_t band, int32_t LM, int32_t bits){
   int32_t i;
   int32_t lo, hi;
   const uint8_t *cache;

   LM++;
   cache = cache_bits50 + cache_index50[LM * m_CELTMode.nbEBands + band];

   lo = 0;
   hi = cache[0];
   bits--;
   for (i=0;i<LOG_MAX_PSEUDO;i++)
   {
      int32_t mid = (lo+hi+1)>>1;
      /* OPT: Make sure this is implemented with a conditional move */
      if ((int32_t)cache[mid] >= bits)
         hi = mid;
      else
         lo = mid;
   }
   if (bits- (lo == 0 ? -1 : (int32_t)cache[lo]) <= (int32_t)cache[hi]-bits)
      return lo;
   else
      return hi;
}

inline int32_t pulses2bits(int32_t band, int32_t LM, int32_t pulses){
   const uint8_t *cache;

   LM++;
   cache = cache_bits50 + cache_index50[LM * m_CELTMode.nbEBands + band];
   return pulses == 0 ? 0 : cache[pulses]+1;
}

inline void smooth_fade(const int16_t *in1, const int16_t *in2,
      int16_t *out, int overlap, int channels,
      const int16_t *window, int32_t Fs)
{
   int i, c;
   int inc = 48000/Fs;
   for (c=0;c<channels;c++)
   {
      for (i=0;i<overlap;i++)
      {
         int16_t w = MULT16_16_Q15(window[i*inc], window[i*inc]);
         out[i*channels+c] = SHR32(MAC16_16(MULT16_16(w,in2[i*channels+c]),
                                   Q15ONE-w, in1[i*channels+c]), 15);
      }
   }
}

void     comb_filter_const(int32_t *y, int32_t *x, int32_t T, int32_t N, int16_t g10, int16_t g11, int16_t g12);
void     comb_filter(int32_t *y, int32_t *x, int32_t T0, int32_t T1, int32_t N, int16_t g0, int16_t g1, int32_t tapset0,
                     int32_t tapset1);
void     init_caps(int32_t *cap, int32_t LM, int32_t C);
uint32_t celt_lcg_rand(uint32_t seed);
int16_t  bitexact_cos(int16_t x);
int32_t  bitexact_log2tan(int32_t isin, int32_t icos);
void     denormalise_bands(const int16_t *X, int32_t *freq, const int16_t *bandLogE, int32_t end, int32_t M,
                           int32_t silence);
void     anti_collapse(int16_t *X_, uint8_t *collapse_masks, int32_t LM, int32_t C, int32_t size, const int16_t *logE,
                       const int16_t *prev1logE, const int16_t *prev2logE, const int32_t *pulses, uint32_t seed);
void     compute_channel_weights(int32_t Ex, int32_t Ey, int16_t w[2]);
void     stereo_split(int16_t *X, int16_t *Y, int32_t N);
void     stereo_merge(int16_t *X, int16_t *Y, int16_t mid, int32_t N);
void     deinterleave_hadamard(int16_t *X, int32_t N0, int32_t stride, int32_t hadamard);
void     interleave_hadamard(int16_t *X, int32_t N0, int32_t stride, int32_t hadamard);
void     haar1(int16_t *X, int32_t N0, int32_t stride);
int32_t  compute_qn(int32_t N, int32_t b, int32_t offset, int32_t pulse_cap, int32_t stereo);
void     compute_theta(struct split_ctx *sctx, int16_t *X, int16_t *Y, int32_t N, int32_t *b, int32_t B, int32_t __B0,
                       int32_t LM, int32_t stereo, int32_t *fill);
uint32_t quant_band_n1(int16_t *X, int16_t *Y, int32_t b, int16_t *lowband_out);
uint32_t quant_partition(int16_t *X, int32_t N, int32_t b, int32_t B, int16_t *lowband, int32_t LM, int16_t gain,
                         int32_t fill);
uint32_t quant_band(int16_t *X, int32_t N, int32_t b, int32_t B, int16_t *lowband, int32_t LM, int16_t *lowband_out,
                    int16_t gain, int16_t *lowband_scratch, int32_t fill);
uint32_t quant_band_stereo(int16_t *X, int16_t *Y, int32_t N, int32_t b, int32_t B, int16_t *lowband, int32_t LM,
                           int16_t *lowband_out, int16_t *lowband_scratch, int32_t fill);
void     special_hybrid_folding(int16_t *norm, int16_t *norm2, int32_t M, int32_t dual_stereo);
void     quant_all_bands(int16_t *X_, int16_t *Y_, uint8_t *collapse_masks, int32_t *pulses, int32_t shortBlocks,
                         int32_t spread, int32_t dual_stereo, int32_t intensity, int32_t *tf_res, int32_t total_bits,
                         int32_t balance, int32_t LM, int32_t codedBands);
int32_t  celt_decoder_get_size(int32_t channels);
int32_t  celt_decoder_init(int32_t channels);
void     deemphasis_stereo_simple(int32_t *in[], int16_t *pcm, int32_t N, const int16_t coef0, int32_t *mem);
void     deemphasis(int32_t *in[], int16_t *pcm, int32_t N);
void     celt_synthesis(int16_t *X, int32_t *out_syn[], int16_t *oldBandE, int32_t C, int32_t isTransient, int32_t LM,
                        int32_t silence);
void     tf_decode(int32_t isTransient, int32_t *tf_res, int32_t LM);
int32_t  celt_decode_with_ec(int16_t *outbuf, int32_t frame_size);
int32_t  celt_decoder_ctl(int32_t request, ...);
int32_t  cwrsi(int32_t _n, int32_t _k, uint32_t _i, int32_t *_y);
int32_t  decode_pulses(int32_t *_y, int32_t _n, int32_t _k);
uint32_t ec_tell_frac();
int32_t  ec_read_byte();
int32_t  ec_read_byte_from_end();
void     ec_dec_normalize();
void     ec_dec_init(uint8_t *_buf, uint32_t _storage);
uint32_t ec_decode(uint32_t _ft);
uint32_t ec_decode_bin(uint32_t _bits);
void     ec_dec_update(uint32_t _fl, uint32_t _fh, uint32_t _ft);
int32_t  ec_dec_bit_logp(uint32_t _logp);
int32_t  ec_dec_icdf(const uint8_t *_icdf, uint32_t _ftb);
uint32_t ec_dec_uint(uint32_t _ft);
uint32_t ec_dec_bits(uint32_t _bits);
void     kf_bfly2(kiss_fft_cpx *Fout, int32_t m, int32_t N);
void     kf_bfly4(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *st, int32_t m, int32_t N, int32_t mm);
void     kf_bfly3(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *st, int32_t m, int32_t N, int32_t mm);
void     kf_bfly5(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_state *st, int32_t m, int32_t N, int32_t mm);
void     opus_fft_impl(const kiss_fft_state *st, kiss_fft_cpx *fout);
uint32_t ec_laplace_get_freq1(uint32_t fs0, int32_t decay);
int32_t  ec_laplace_decode(uint32_t fs, int32_t decay);
uint32_t isqrt32(uint32_t _val);
int16_t  celt_rsqrt_norm(int32_t x);
int32_t  celt_sqrt(int32_t x);
int16_t  celt_cos_norm(int32_t x);
int32_t  celt_rcp(int32_t x);
void     clt_mdct_backward(int32_t *in, int32_t *out, int32_t overlap, int32_t shift, int32_t stride);
void     exp_rotation1(int16_t *X, int32_t len, int32_t stride, int16_t c, int16_t s);
void     exp_rotation(int16_t *X, int32_t len, int32_t dir, int32_t stride, int32_t K, int32_t spread);
void     normalise_residual(int32_t *iy, int16_t *X, int32_t N, int32_t Ryy, int16_t gain);
uint32_t extract_collapse_mask(int32_t *iy, int32_t N, int32_t B);
uint32_t alg_unquant(int16_t *X, int32_t N, int32_t K, int32_t spread, int32_t B, int16_t gain);
void     renormalise_vector(int16_t *X, int32_t N, int16_t gain);
int32_t  interp_bits2pulses(int32_t end, int32_t skip_start, const int32_t *bits1, const int32_t *bits2,
                            const int32_t *thresh, const int32_t *cap, int32_t total, int32_t *_balance,
                            int32_t skip_rsv, int32_t *intensity, int32_t intensity_rsv, int32_t *dual_stereo,
                            int32_t dual_stereo_rsv, int32_t *bits, int32_t *ebits, int32_t *fine_priority, int32_t C,
                            int32_t LM);
int32_t  clt_compute_allocation(const int32_t *offsets, const int32_t *cap, int32_t alloc_trim, int32_t *intensity,
                                int32_t *dual_stereo, int32_t total, int32_t *balance, int32_t *pulses, int32_t *ebits,
                                int32_t *fine_priority, int32_t C, int32_t LM);
void     unquant_coarse_energy(int16_t *oldEBands, int32_t intra, int32_t C, int32_t LM);
void     unquant_fine_energy(int16_t *oldEBands, int32_t *fine_quant, int32_t C);
void     unquant_energy_finalise(int16_t *oldEBands, int32_t *fine_quant, int32_t *fine_priority, int32_t bits_left,
                                 int32_t C);
uint32_t celt_pvq_u_row(uint32_t row, uint32_t data);

bool     CELTDecoder_AllocateBuffers(void);
void     CELTDecoder_FreeBuffers();
void     CELTDecoder_ClearBuffer(void);

