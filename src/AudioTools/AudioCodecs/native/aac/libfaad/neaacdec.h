/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2 must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**/

// ESP32 Version 29.07.1961
// updated:      02.01.2025

#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>


/* ----------------------COMPILE TIME DEFINITIONS ---------------- */
#define PREFER_POINTERS // Use if target platform has address generators with autoincrement
// #define BIG_IQ_TABLE
// #define USE_DOUBLE_PRECISION // use double precision
#define FIXED_POINT          // use fixed point reals, undefs MAIN_DEC and SSR_DEC
#define ERROR_RESILIENCE 2
#define MAIN_DEC // Allow decoding of MAIN profile AAC
// #define SSR_DEC // Allow decoding of SSR profile AAC
#define LTP_DEC // Allow decoding of LTP (Long Term Prediction) profile AAC
#define LD_DEC  // Allow decoding of LD (Low Delay) profile AAC
// #define DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
#if (defined CONFIG_IDF_TARGET_ESP32S3 && defined BOARD_HAS_PSRAM)
    #define SBR_DEC // Allow decoding of SBR (Spectral Band Replication) profile AAC
    #define PS_DEC // Allow decoding of PS (Parametric Stereo) profile AAC
#endif
// #define SBR_LOW_POWER
#define ALLOW_SMALL_FRAMELENGTH
// #define LC_ONLY_DECODER // if you want a pure AAC LC decoder (independant of SBR_DEC and PS_DEC)
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef DRM_SUPPORT // Allow decoding of Digital Radio Mondiale (DRM)
    #define DRM
    #define DRM_PS
#endif
#ifdef LD_DEC /* LD can't do without LTP */
    #ifndef ERROR_RESILIENCE
        #define ERROR_RESILIENCE
    #endif
    #ifndef LTP_DEC
        #define LTP_DEC
    #endif
#endif
#ifdef LC_ONLY_DECODER
    #undef LD_DEC
    #undef LTP_DEC
    #undef MAIN_DEC
    #undef SSR_DEC
    #undef DRM
    #undef DRM_PS
    #undef ALLOW_SMALL_FRAMELENGTH
    #undef ERROR_RESILIENCE
#endif
#ifdef SBR_LOW_POWER
    #undef PS_DEC
#endif
#ifdef FIXED_POINT /*  No MAIN decoding */
    #ifdef MAIN_DEC
        #undef MAIN_DEC
    #endif
#endif // FIXED_POINT
#ifdef DRM
    #ifndef ALLOW_SMALL_FRAMELENGTH
        #define ALLOW_SMALL_FRAMELENGTH
    #endif
    #undef LD_DEC
    #undef LTP_DEC
    #undef MAIN_DEC
    #undef SSR_DEC
#endif
/* END COMPILE TIME DEFINITIONS */


#ifdef WORDS_BIGENDIAN
    #define ARCH_IS_BIG_ENDIAN
#endif
/* FIXED_POINT doesn't work with MAIN and SSR yet */
#ifdef FIXED_POINT
    #undef MAIN_DEC
    #undef SSR_DEC
#endif
#if defined(FIXED_POINT)
#elif defined(USE_DOUBLE_PRECISION)
#else                                   /* Normal floating point operation */
    #ifdef HAVE_LRINTF
        #define HAS_LRINTF
        #define _ISOC9X_SOURCE 1
        #define _ISOC99_SOURCE 1
        #define __USE_ISOC9X   1
        #define __USE_ISOC99   1
    #endif


#endif
#ifndef HAS_LRINTF
/* standard cast */
// #define int32_t(f) ((int32_t)(f))
#endif
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
/* defines if an object type can be decoded by this library or not */
__unused static uint8_t ObjectTypesTable[32] = {
    0, /*  0 NULL */
#ifdef MAIN_DEC
    1, /*  1 AAC Main */
#else
    0, /*  1 AAC Main */
#endif
    1, /*  2 AAC LC */
#ifdef SSR_DEC
    1, /*  3 AAC SSR */
#else
    0, /*  3 AAC SSR */
#endif
#ifdef LTP_DEC
    1, /*  4 AAC LTP */
#else
    0, /*  4 AAC LTP */
#endif
#ifdef SBR_DEC
    1, /*  5 SBR */
#else
    0, /*  5 SBR */
#endif
    0, /*  6 AAC Scalable */
    0, /*  7 TwinVQ */
    0, /*  8 CELP */
    0, /*  9 HVXC */
    0, /* 10 Reserved */
    0, /* 11 Reserved */
    0, /* 12 TTSI */
    0, /* 13 Main synthetic */
    0, /* 14 Wavetable synthesis */
    0, /* 15 General MIDI */
    0, /* 16 Algorithmic Synthesis and Audio FX */
/* MPEG-4 Version 2 */
#ifdef ERROR_RESILIENCE
    1, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    #ifdef LTP_DEC
    1, /* 19 ER AAC LTP */
    #else
    0, /* 19 ER AAC LTP */
    #endif
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    #ifdef LD_DEC
    1, /* 23 ER AAC LD */
    #else
    0, /* 23 ER AAC LD */
    #endif
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#else  /* No ER defined */
    0, /* 17 ER AAC LC */
    0, /* 18 (Reserved) */
    0, /* 19 ER AAC LTP */
    0, /* 20 ER AAC scalable */
    0, /* 21 ER TwinVQ */
    0, /* 22 ER BSAC */
    0, /* 23 ER AAC LD */
    0, /* 24 ER CELP */
    0, /* 25 ER HVXC */
    0, /* 26 ER HILN */
    0, /* 27 ER Parametric */
#endif
    0, /* 28 (Reserved) */
#ifdef PS_DEC
    1, /* 29 AAC LC + SBR + PS */
#else
    0, /* 29 AAC LC + SBR + PS */
#endif
    0, /* 30 (Reserved) */
    0  /* 31 (Reserved) */
};
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

#define ZERO_HCB              0
#define FIRST_PAIR_HCB        5
#define ESC_HCB               11
#define QUAD_LEN              4
#define PAIR_LEN              2
#define NOISE_HCB             13
#define INTENSITY_HCB2        14
#define INTENSITY_HCB         15
#define DRC_REF_LEVEL         20 * 4 /* -20 dB */
#define DRM_PARAMETRIC_STEREO 0
#define DRM_NUM_SA_BANDS      8
#define DRM_NUM_PAN_BANDS     20
#define NUM_OF_LINKS          3
#define NUM_OF_QMF_CHANNELS   64
#define NUM_OF_SUBSAMPLES     30
#define MAX_SA_BAND           46
#define MAX_PAN_BAND          64
#define MAX_DELAY             5
#define EXTENSION_ID_PS       2
#define MAX_PS_ENVELOPES      5
#define NO_ALLPASS_LINKS      3
#define BYTE_NUMBIT           8
#define BYTE_NUMBIT_LD        3
#define bit2byte(a)           ((a + 7) >> BYTE_NUMBIT_LD)
#define NUM_ERROR_MESSAGES    34
#define ESC_VAL               7
#define SSR_BANDS             4
#define PQFTAPS               96

#ifdef DRM
    #define DECAY_CUTOFF 3
    #define DECAY_SLOPE  0.05f
/* type definitaions */
typedef const int8_t (*drm_ps_huff_tab)[2];
#endif

#define FLOAT_SCALE   (1.0f / (1 << 15))
#define DM_MUL        REAL_CONST(0.3203772410170407)    // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2        REAL_CONST(0.7071067811865475244) // 1/sqrt(2)
#define NUM_CB        6
#define NUM_CB_ER     22
#define MAX_CB        32
#define VCB11_FIRST   16
#define VCB11_LAST    31
#define TNS_MAX_ORDER 20
#define MAIN          1
#define LC            2
#define SSR           3
#define LTP           4
#define HE_AAC        5
#define LD            23
#define ER_LC         17
#define ER_LTP        19
#define DRM_ER_LC     27 /* special object type for DRM */
/* header types */
#define RAW  0
#define ADIF 1
#define ADTS 2
#define LATM 3
/* SBR signalling */
#define NO_SBR           0
#define SBR_UPSAMPLED    1
#define SBR_DOWNSAMPLED  2
#define NO_SBR_UPSAMPLED 3
/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_STEREO    4
#define DRMCH_SBR_PS_STEREO 5
/* First object type that has ER */
#define ER_OBJECT_START 17
/* Bitstream */
#define LEN_SE_ID         3
#define LEN_TAG           4
#define LEN_BYTE          8
#define EXT_FIL           0
#define EXT_FILL_DATA     1
#define EXT_DATA_ELEMENT  2
#define EXT_DYNAMIC_RANGE 11
#define ANC_DATA          0
/* Syntax elements */
#define ID_SCE               0x0
#define ID_CPE               0x1
#define ID_CCE               0x2
#define ID_LFE               0x3
#define ID_DSE               0x4
#define ID_PCE               0x5
#define ID_FIL               0x6
#define ID_END               0x7
#define INVALID_ELEMENT_ID   255
#define ONLY_LONG_SEQUENCE   0x0
#define LONG_START_SEQUENCE  0x1
#define EIGHT_SHORT_SEQUENCE 0x2
#define LONG_STOP_SEQUENCE   0x3
#define ZERO_HCB             0
#define FIRST_PAIR_HCB       5
#define ESC_HCB              11
#define QUAD_LEN             4
#define PAIR_LEN             2
#define NOISE_HCB            13
#define INTENSITY_HCB2       14
#define INTENSITY_HCB        15
#define INVALID_SBR_ELEMENT  255
#define T_HFGEN              8
#define T_HFADJ              2
#define EXT_SBR_DATA         13
#define EXT_SBR_DATA_CRC     14
#define FIXFIX               0
#define FIXVAR               1
#define VARFIX               2
#define VARVAR               3
#define LO_RES               0
#define HI_RES               1
#define NO_TIME_SLOTS_960    15
#define NO_TIME_SLOTS        16
#define RATE                 2
#define NOISE_FLOOR_OFFSET   6

#ifdef PS_DEC
    #define NEGATE_IPD_MASK (0x1000)
    #define DECAY_SLOPE     FRAC_CONST(0.05)
    #define COEF_SQRT2      COEF_CONST(1.4142135623731)
#endif //  PS_DEC

#define MAX_NTSRHFG 40 /* MAX_NTSRHFG: maximum of number_time_slots * rate + HFGen. 16*2+8 */
#define MAX_NTSR    32 /* max number_time_slots * rate, ok for DRM and not DRM mode */
#define MAX_M       49 /* MAX_M: maximum value for M */
#define MAX_L_E     5  /* MAX_L_E: maximum value for L_E */

#ifdef SBR_DEC
    #ifdef FIXED_POINT
        #define _EPS (1) /* smallest number available in fixed point */
    #else
        #define _EPS (1e-12)
    #endif
#endif // SBR_DEC

#ifdef FIXED_POINT /* int32_t */
    #define LOG2_MIN_INF REAL_CONST(-10000)
    #define COEF_BITS      28
    #define COEF_PRECISION (1 << COEF_BITS)
    #define REAL_BITS      14 // MAXIMUM OF 14 FOR FIXED POINT SBR
    #define REAL_PRECISION (1 << REAL_BITS)
    /* FRAC is the fractional only part of the fixed point number [0.0..1.0) */
    #define FRAC_SIZE      32 /* frac is a 32 bit integer */
    #define FRAC_BITS      31
    #define FRAC_PRECISION ((uint32_t)(1 << FRAC_BITS))
    #define FRAC_MAX       0x7FFFFFFF
    typedef int32_t real_t;
    #define REAL_CONST(A) (((A) >= 0) ? ((real_t)((A) * (REAL_PRECISION) + 0.5)) : ((real_t)((A) * (REAL_PRECISION) - 0.5)))
    #define COEF_CONST(A) (((A) >= 0) ? ((real_t)((A) * (COEF_PRECISION) + 0.5)) : ((real_t)((A) * (COEF_PRECISION) - 0.5)))
    #define FRAC_CONST(A) (((A) == 1.00) ? ((real_t)FRAC_MAX) : (((A) >= 0) ? ((real_t)((A) * (FRAC_PRECISION) + 0.5)) : ((real_t)((A) * (FRAC_PRECISION) - 0.5))))
    // #define FRAC_CONST(A) (((A) >= 0) ? ((real_t)((A)*(FRAC_PRECISION)+0.5)) : ((real_t)((A)*(FRAC_PRECISION)-0.5)))
    #define Q2_BITS      22
    #define Q2_PRECISION (1 << Q2_BITS)
    #define Q2_CONST(A)  (((A) >= 0) ? ((real_t)((A) * (Q2_PRECISION) + 0.5)) : ((real_t)((A) * (Q2_PRECISION) - 0.5)))
    /* multiply with real shift */
    #define MUL_R(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (REAL_BITS - 1))) >> REAL_BITS)
    /* multiply with coef shift */
    #define MUL_C(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (COEF_BITS - 1))) >> COEF_BITS)
    /* multiply with fractional shift */
    #define _MulHigh(A, B)    (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (FRAC_SIZE - 1))) >> FRAC_SIZE)
    #define MUL_F(A, B)       (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (FRAC_BITS - 1))) >> FRAC_BITS)
    #define MUL_Q2(A, B)      (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (Q2_BITS - 1))) >> Q2_BITS)
    #define MUL_SHIFT6(A, B)  (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (6 - 1))) >> 6)
    #define MUL_SHIFT23(A, B) (real_t)(((int64_t)(A) * (int64_t)(B) + (1 << (23 - 1))) >> 23)
    #define DIV_R(A, B)       (((int64_t)A << REAL_BITS) / B)
    #define DIV_C(A, B)       (((int64_t)A << COEF_BITS) / B)



/*    Complex multiplication */
//    static inline void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) { // FIXED POINT
//        *y1 = (_MulHigh(x1, c1) + _MulHigh(x2, c2)) << (FRAC_SIZE - FRAC_BITS);
//        *y2 = (_MulHigh(x2, c1) - _MulHigh(x1, c2)) << (FRAC_SIZE - FRAC_BITS);
//    }
static inline void ComplexMult(int32_t* y1, int32_t* y2, int32_t x1, int32_t x2, int32_t c1, int32_t c2) {
    asm volatile (
        //  y1 = (x1 * c1) + (x2 * c2)
        "mulsh a2, %2, %4\n"        // a2 = x1 * c1 (Low 32 bits)
        "mulsh a3, %3, %5\n"        // a3 = x2 * c2 (Low 32 bits)
        "add   a2, a2, a3\n"        // a2 = (x1 * c1) + (x2 * c2)
        "slli  a2, a2,  1\n"        // a2 = a2 >> 31 (Fixed-Point scaling)
        "s32i  a2, %0   \n"         // Store result in *y1
        // y2 = (x2 * c1) - (x1 * c2)
        "mulsh a2, %3, %4\n"        // a2 = x2 * c1 (Low 32 bits)
        "mulsh a3, %2, %5\n"        // a3 = x1 * c2 (Low 32 bits)
        "sub   a2, a2, a3\n"        // a2 = (x2 * c1) - (x1 * c2)
        "slli  a2, a2,  1\n"        // a2 = a2 >> 31 (Fixed-Point scaling)
        "s32i  a2, %1    \n"        // Store result in *y2
        : "=m" (*y1), "=m" (*y2)                  // Output
        : "r" (x1), "r" (x2), "r" (c1), "r" (c2)  // Input
        : "a2", "a3"                              // Clobbers
    );
}


    #define DIV(A, B) (((int64_t)A << REAL_BITS) / B)
    #define step(shift)                                  \
        if ((0x40000000l >> shift) + root <= value) {    \
            value -= (0x40000000l >> shift) + root;      \
            root = (root >> 1) | (0x40000000l >> shift); \
        } else {                                         \
            root = root >> 1;                            \
        }


real_t const pow2_table[] = {COEF_CONST(1.0), COEF_CONST(1.18920711500272), COEF_CONST(1.41421356237310), COEF_CONST(1.68179283050743)};
#endif // FIXED_POINT
#ifndef FIXED_POINT
    #ifdef MAIN_DEC
    #define ALPHA         REAL_CONST(0.90625)
    #define A             REAL_CONST(0.953125)
    #endif
    #define IQ_TABLE_SIZE 8192
    #define DIV_R(A, B)   ((A) / (B))
    #define DIV_C(A, B)   ((A) / (B))
    #ifdef USE_DOUBLE_PRECISION /* double */
typedef double real_t;
        #include <math.h>
        #define MUL_R(A, B) ((A) * (B))
        #define MUL_C(A, B) ((A) * (B))
        #define MUL_F(A, B) ((A) * (B))
/* Complex multiplication */
static void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) {
    *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
    *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
}
        #define REAL_CONST(A) ((real_t)(A))
        #define COEF_CONST(A) ((real_t)(A))
        #define Q2_CONST(A)   ((real_t)(A))
 
       #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */
    #else                                   /* Normal floating point operation */
typedef float real_t;
        #define MUL_R(A, B)   ((A) * (B))
        #define MUL_C(A, B)   ((A) * (B))
        #define MUL_F(A, B)   ((A) * (B))
        #define REAL_CONST(A) ((real_t)(A))
        #define COEF_CONST(A) ((real_t)(A))
        #define Q2_CONST(A)   ((real_t)(A))
        #define FRAC_CONST(A) ((real_t)(A)) /* pure fractional part */
    /* Complex multiplication */
        static void ComplexMult(real_t* y1, real_t* y2, real_t x1, real_t x2, real_t c1, real_t c2) {
            *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
            *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
        }

    #endif                                  /* USE_DOUBLE_PRECISION */
#endif                                      // FIXED_POINT

#ifdef SBR_LOW_POWER
    #define qmf_t     real_t
    #define QMF_RE(A) (A)
    #define QMF_IM(A)
#else
    #define qmf_t     complex_t
    #define QMF_RE(A) RE(A)
    #define QMF_IM(A) IM(A)
#endif
typedef real_t complex_t[2];
#define RE(A) A[0]
#define IM(A) A[1]

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#ifndef max
    #define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
    #define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
 #include "structs.h"
 #include "tables.h"
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————



#ifndef FAAD2_VERSION
  #define FAAD2_VERSION "unknown"
#endif
/* object types for AAC */
#define MAIN       1
#define LC         2
#define SSR        3
#define LTP        4
#define HE_AAC     5
#define ER_LC     17
#define ER_LTP    19
#define LD        23
#define DRM_ER_LC 27 /* special object type for DRM */
/* header types */
#define RAW        0
#define ADIF       1
#define ADTS       2
#define LATM       3
/* SBR signalling */
#define NO_SBR           0
#define SBR_UPSAMPLED    1
#define SBR_DOWNSAMPLED  2
#define NO_SBR_UPSAMPLED 3
/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_FIXED  FAAD_FMT_FLOAT
#define FAAD_FMT_DOUBLE 5
/* Capabilities */
#define LC_DEC_CAP           (1<<0) /* Can decode LC */
#define MAIN_DEC_CAP         (1<<1) /* Can decode MAIN */
#define LTP_DEC_CAP          (1<<2) /* Can decode LTP */
#define LD_DEC_CAP           (1<<3) /* Can decode LD */
#define ERROR_RESILIENCE_CAP (1<<4) /* Can decode ER */
#define FIXED_POINT_CAP      (1<<5) /* Fixed point */
/* Channel definitions */
#define FRONT_CHANNEL_CENTER (1)
#define FRONT_CHANNEL_LEFT   (2)
#define FRONT_CHANNEL_RIGHT  (3)
#define SIDE_CHANNEL_LEFT    (4)
#define SIDE_CHANNEL_RIGHT   (5)
#define BACK_CHANNEL_LEFT    (6)
#define BACK_CHANNEL_RIGHT   (7)
#define BACK_CHANNEL_CENTER  (8)
#define LFE_CHANNEL          (9)
#define UNKNOWN_CHANNEL      (0)
/* DRM channel definitions */
#define DRMCH_MONO          1
#define DRMCH_STEREO        2
#define DRMCH_SBR_MONO      3
#define DRMCH_SBR_STEREO    4
#define DRMCH_SBR_PS_STEREO 5
/* A decode call can eat up to FAAD_MIN_STREAMSIZE bytes per decoded channel,
   so at least so much bytes per channel should be available in this stream */
#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */
typedef void *NeAACDecHandle;
typedef struct mp4AudioSpecificConfig
{
    /* Audio Specific Info */
    unsigned char objectTypeIndex;
    unsigned char samplingFrequencyIndex;
    uint32_t samplingFrequency;
    unsigned char channelsConfiguration;
    /* GA Specific Info */
    unsigned char frameLengthFlag;
    unsigned char dependsOnCoreCoder;
    unsigned short coreCoderDelay;
    unsigned char extensionFlag;
    unsigned char aacSectionDataResilienceFlag;
    unsigned char aacScalefactorDataResilienceFlag;
    unsigned char aacSpectralDataResilienceFlag;
    unsigned char epConfig;
    char sbr_present_flag;
    char forceUpSampling;
    char downSampledSBR;
} mp4AudioSpecificConfig;
typedef struct NeAACDecFrameInfo
{
    uint32_t bytesconsumed;
    uint32_t samples;
    unsigned char channels;
    unsigned char error;
    uint32_t samplerate;
    /* SBR: 0: off, 1: on; upsample, 2: on; downsampled, 3: off; upsampled */
    unsigned char sbr;
    /* MPEG-4 ObjectType */
    unsigned char object_type;
    /* AAC header type; MP4 will be signalled as RAW also */
    unsigned char header_type;
    /* multichannel configuration */
    unsigned char num_front_channels;
    unsigned char num_side_channels;
    unsigned char num_back_channels;
    unsigned char num_lfe_channels;
    unsigned char channel_position[64];
    /* PS: 0: off, 1: on */
    unsigned char ps;
    uint8_t  isPS;
} NeAACDecFrameInfo;

uint8_t  cpu_has_sse(void);
uint32_t ne_rng(uint32_t* __r1, uint32_t* __r2);
uint32_t wl_min_lzc(uint32_t x);
#ifdef FIXED_POINT
int32_t log2_int(uint32_t val);
int32_t log2_fix(uint32_t val);
int32_t pow2_int(real_t val);
real_t  pow2_fix(real_t val);
#endif
uint8_t  get_sr_index(const uint32_t samplerate);
uint8_t  max_pred_sfb(const uint8_t sr_index);
uint8_t  max_tns_sfb(const uint8_t sr_index, const uint8_t object_type, const uint8_t is_short);
uint32_t get_sample_rate(const uint8_t sr_index);
int8_t   can_decode_ot(const uint8_t object_type);
void*    faad_malloc(size_t size);
template <typename freeType>
void     faad_free(freeType** b);

drc_info*                drc_init(real_t cut, real_t boost);
void                     drc_end(drc_info* drc);
void                     drc_decode(drc_info* drc, real_t* spec);
sbr_info*                sbrDecodeInit(uint16_t framelength, uint8_t id_aac, uint32_t sample_rate, uint8_t downSampledSBR, uint8_t IsDRM);
void                     sbrDecodeEnd(sbr_info* sbr);
void                     sbrReset(sbr_info* sbr);
uint8_t                  sbrDecodeCoupleFrame(sbr_info* sbr, real_t* left_chan, real_t* right_chan, const uint8_t just_seeked, const uint8_t downSampledSBR);
uint8_t                  sbrDecodeSingleFrame(sbr_info* sbr, real_t* channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
uint16_t                 ps_data(ps_info* ps, bitfile* ld, uint8_t* header);
ps_info*                 ps_init(uint8_t sr_index, uint8_t numTimeSlotsRate);
void                     ps_free(ps_info* ps);
uint8_t                  ps_decode(ps_info* ps, qmf_t X_left[38][64], qmf_t X_right[38][64]);
void                     faad_initbits(bitfile* ld, const void* buffer, const uint32_t buffer_size);
void                     faad_endbits(bitfile* ld);
void                     faad_initbits_rev(bitfile* ld, void* buffer, uint32_t bits_in_buffer);
uint8_t                  faad_byte_align(bitfile* ld);
uint32_t                 faad_get_processed_bits(bitfile* ld);
void                     faad_flushbits_ex(bitfile* ld, uint32_t bits);
void                     faad_rewindbits(bitfile* ld);
void                     faad_resetbits(bitfile* ld, int bits);
uint8_t*                 faad_getbitbuffer(bitfile* ld, uint32_t bits);
void*                    faad_origbitbuffer(bitfile* ld);
uint32_t                 faad_origbitbuffer_size(bitfile* ld);
uint8_t                  faad_get1bit(bitfile* ld);
uint32_t                 faad_getbits(bitfile* ld, uint32_t n);
uint32_t                 faad_showbits_rev(bitfile* ld, uint32_t bits);
void                     faad_flushbits_rev(bitfile* ld, uint32_t bits);
uint32_t                 getdword(void* mem);
uint32_t                 getdword_n(void* mem, int n);
void                     faad_flushbits(bitfile* ld, uint32_t bits);
uint32_t                 faad_showbits(bitfile* ld, uint32_t bits);
uint32_t                 showbits_hcr(bits_t* ld, uint8_t bits);
uint32_t                 faad_getbits_rev(bitfile* ld, uint32_t n);
int8_t                   get1bit_hcr(bits_t* ld, uint8_t* result);
int8_t                   flushbits_hcr(bits_t* ld, uint8_t bits);
int8_t                   getbits_hcr(bits_t* ld, uint8_t n, uint32_t* result);
void                     cfftf(cfft_info* cfft, complex_t* c);
void                     cfftb(cfft_info* cfft, complex_t* c);
cfft_info*               cffti(uint16_t n);
void                     cfftu(cfft_info* cfft);
NeAACDecHandle           NeAACDecOpen(void);
const char*              NeAACDecGetErrorMessage(unsigned const char errcode);
void*                    NeAACDecDecode2(NeAACDecHandle hpDecoder, NeAACDecFrameInfo* hInfo, unsigned char* buffer, uint32_t buffer_size, void** sample_buffer, uint32_t sample_buffer_size);
long                     NeAACDecInit(NeAACDecHandle hpDecoder, unsigned char* buffer, uint32_t buffer_size, uint32_t* samplerate, unsigned char* channels);
unsigned char            NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr config);
char                     NeAACDecInit2(NeAACDecHandle hpDecoder, unsigned char* pBuffer, uint32_t SizeOfDecoderSpecificInfo, uint32_t* samplerate, unsigned char* channels);
unsigned char            NeAACDecSetConfiguration(NeAACDecHandle hpDecoder, NeAACDecConfigurationPtr config);
void                     NeAACDecClose(NeAACDecHandle hpDecoder);
NeAACDecConfigurationPtr NeAACDecGetCurrentConfiguration(NeAACDecHandle hpDecoder);
void* aac_frame_decode(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, unsigned char* buffer, uint32_t buffer_size, void** sample_buffer2, uint32_t sample_buffer_size);
void  create_channel_config(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo);
void  ssr_filter_bank_end(fb_info* fb);
void  passf2pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
void  passf2neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa);
void  passf3(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const int8_t isign);
void  passf4pos(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
void  passf4neg(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3);
void  passf5(const uint16_t ido, const uint16_t l1, const complex_t* cc, complex_t* ch, const complex_t* wa1, const complex_t* wa2, const complex_t* wa3, const complex_t* wa4, const int8_t isign);
void  cffti1(uint16_t n, complex_t* wa, uint16_t* ifac);
drc_info *drc_init(real_t cut, real_t boost);
void drc_end(drc_info *drc);
void drc_decode(drc_info *drc, real_t *spec);
fb_info* filter_bank_init(uint16_t frame_len);
void     filter_bank_end(fb_info* fb);
void filter_bank_ltp(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* in_data, real_t* out_mdct, uint8_t object_type, uint16_t frame_len);
void ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* freq_in, real_t* time_out, real_t* overlap, uint8_t object_type, uint16_t frame_len);
void ms_decode(ic_stream *ics, ic_stream *icsr, real_t *l_spec, real_t *r_spec, uint16_t frame_len);
void is_decode(ic_stream* ics, ic_stream* icsr, real_t* l_spec, real_t* r_spec, uint16_t frame_len);
int8_t is_intensity(ic_stream* ics, uint8_t group, uint8_t sfb);
uint8_t is_noise(ic_stream *ics, uint8_t group, uint8_t sfb);
real_t fp_sqrt(real_t value);
void pns_decode(ic_stream* ics_left, ic_stream* ics_right, real_t* spec_left, real_t* spec_right, uint16_t frame_len, uint8_t channel_pair, uint8_t object_type,
                /* RNG states */ uint32_t* __r1, uint32_t* __r2);
int8_t invert_intensity(ic_stream* ics, uint8_t group, uint8_t sfb);
void* output_to_PCM(NeAACDecStruct* hDecoder, real_t** input, void* samplebuffer, uint8_t channels, uint16_t frame_len, uint8_t format);
uint8_t pulse_decode(ic_stream *ics, int16_t *spec_coef, uint16_t framelen);
void gen_rand_vector(real_t* spec, int16_t scale_factor, uint16_t size, uint8_t sub, uint32_t* __r1, uint32_t* __r2);
void huffman_sign_bits(bitfile *ld, int16_t *sp, uint8_t len);
uint8_t huffman_getescape(bitfile *ld, int16_t *sp);
uint8_t huffman_2step_quad(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_2step_pair(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_2step_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_binary_quad(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_binary_pair(uint8_t cb, bitfile *ld, int16_t *sp);
uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
int16_t huffman_codebook(uint8_t i);
void vcb11_check_LAV(uint8_t cb, int16_t *sp);
uint16_t drm_ps_data(drm_ps_info *ps, bitfile *ld);
drm_ps_info *drm_ps_init(void);
void drm_ps_free(drm_ps_info *ps);
uint8_t drm_ps_decode(drm_ps_info *ps, uint8_t guess, qmf_t X_left[38][64], qmf_t X_right[38][64]);
int8_t huffman_scale_factor(bitfile *ld);
uint8_t huffman_spectral_data(uint8_t cb, bitfile *ld, int16_t *sp);
int8_t huffman_spectral_data_2(uint8_t cb, bits_t *ld, int16_t *sp);
fb_info* ssr_filter_bank_init(uint16_t frame_len);
void     ssr_filter_bank_end(fb_info* fb);
void ssr_ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* freq_in, real_t* time_out, uint16_t frame_len);
int8_t AudioSpecificConfig2(uint8_t* pBuffer, uint32_t buffer_size, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint8_t short_form);
int8_t AudioSpecificConfigFromBitfile(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce, uint32_t bsize, uint8_t short_form);
void pns_reset_pred_state(ic_stream* ics, pred_state* state);
void reset_all_predictors(pred_state* state, uint16_t frame_len);
void ic_prediction(ic_stream* ics, real_t* spec, pred_state* state, uint16_t frame_len, uint8_t sf_index);
uint8_t quant_to_spec(NeAACDecStruct* hDecoder, ic_stream* ics, int16_t* quant_data, real_t* spec_data, uint16_t frame_len);
uint8_t window_grouping_info(NeAACDecStruct* hDecoder, ic_stream* ics);
uint8_t reconstruct_channel_pair(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, element* cpe, int16_t* spec_data1, int16_t* spec_data2);
uint8_t reconstruct_single_channel(NeAACDecStruct* hDecoder, ic_stream* ics, element* sce, int16_t* spec_data);
void tns_decode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, real_t* spec, uint16_t frame_len);
void tns_encode_frame(ic_stream* ics, tns_info* tns, uint8_t sr_index, uint8_t object_type, real_t* spec, uint16_t frame_len);
uint8_t is_ltp_ot(uint8_t object_type);
void    lt_prediction(ic_stream* ics, ltp_info* ltp, real_t* spec, int16_t* lt_pred_stat, fb_info* fb, uint8_t win_shape, uint8_t win_shape_prev, uint8_t sr_index, uint8_t object_type,
                      uint16_t frame_len);
void    lt_update_state(int16_t* lt_pred_stat, real_t* time, real_t* overlap, uint16_t frame_len, uint8_t object_type);
void tns_decode_coef(uint8_t order, uint8_t coef_res_bits, uint8_t coef_compress, uint8_t* coef, real_t* a);
void tns_ar_filter(real_t* spectrum, uint16_t size, int8_t inc, real_t* lpc, uint8_t order);
void tns_ma_filter(real_t* spectrum, uint16_t size, int8_t inc, real_t* lpc, uint8_t order);
uint8_t faad_check_CRC(bitfile* ld, uint16_t len);
/* static function declarations */
void    decode_sce_lfe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele);
void    decode_cpe(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, uint8_t id_syn_ele);
uint8_t single_lfe_channel_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channel, uint8_t* tag);
uint8_t channel_pair_element(NeAACDecStruct* hDecoder, bitfile* ld, uint8_t channel, uint8_t* tag);
#ifdef COUPLING_DEC
uint8_t coupling_channel_element(NeAACDecStruct* hDecoder, bitfile* ld);
#endif
uint16_t data_stream_element(NeAACDecStruct* hDecoder, bitfile* ld);
uint8_t  program_config_element(program_config* pce, bitfile* ld);
uint8_t  fill_element(NeAACDecStruct* hDecoder, bitfile* ld, drc_info* drc,  uint8_t sbr_ele);
uint8_t individual_channel_stream(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag, int16_t* spec_data);
uint8_t ics_info(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, uint8_t common_window);
uint8_t section_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld);
uint8_t scale_factor_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld);
#ifdef SSR_DEC
void gain_control_data(bitfile* ld, ic_stream* ics);
#endif
uint8_t  spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data);
uint16_t extension_payload(bitfile* ld, drc_info* drc, uint16_t count);
uint8_t  pulse_data(ic_stream* ics, pulse_info* pul, bitfile* ld);
void     tns_data(ic_stream* ics, tns_info* tns, bitfile* ld);
#ifdef LTP_DEC
uint8_t ltp_data(NeAACDecStruct* hDecoder, ic_stream* ics, ltp_info* ltp, bitfile* ld);
#endif
uint8_t adts_fixed_header(adts_header* adts, bitfile* ld);
void    adts_variable_header(adts_header* adts, bitfile* ld);
void    adts_error_check(adts_header* adts, bitfile* ld);
uint8_t dynamic_range_info(bitfile* ld, drc_info* drc);
uint8_t excluded_channels(bitfile* ld, drc_info* drc);
uint8_t side_info(NeAACDecStruct* hDecoder, element* ele, bitfile* ld, ic_stream* ics, uint8_t scal_flag);
int8_t GASpecificConfig(bitfile* ld, mp4AudioSpecificConfig* mp4ASC, program_config* pce);
uint8_t adts_frame(adts_header* adts, bitfile* ld);
void    get_adif_header(adif_header* adif, bitfile* ld);
void    raw_data_block(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc);
uint8_t reordered_spectral_data(NeAACDecStruct* hDecoder, ic_stream* ics, bitfile* ld, int16_t* spectral_data);
#ifdef DRM
int8_t DRM_aac_scalable_main_header(NeAACDecStruct* hDecoder, ic_stream* ics1, ic_stream* ics2, bitfile* ld, uint8_t this_layer_stereo);
#endif
void dct4_kernel(real_t * in_real, real_t * in_imag, real_t * out_real, real_t * out_imag);
void DCT3_32_unscaled(real_t *y, real_t *x);
void DCT4_32(real_t *y, real_t *x);
void DST4_32(real_t *y, real_t *x);
void DCT2_32_unscaled(real_t *y, real_t *x);
void DCT4_16(real_t *y, real_t *x);
void DCT2_16_unscaled(real_t *y, real_t *x);
uint8_t rvlc_scale_factor_data(ic_stream *ics, bitfile *ld);
uint8_t rvlc_decode_scale_factors(ic_stream *ics, bitfile *ld);
uint8_t sbr_extension_data(bitfile* ld, sbr_info* sbr, uint16_t cnt, uint8_t resetFlag);
int8_t rvlc_huffman_sf(bitfile* ld_sf, bitfile* ld_esc, int8_t direction);
int8_t rvlc_huffman_esc(bitfile* ld_esc, int8_t direction);
uint8_t rvlc_decode_sf_forward(ic_stream* ics, bitfile* ld_sf, bitfile* ld_esc, uint8_t* intensity_used);
#ifdef DRM
void DRM_aac_scalable_main_element(NeAACDecStruct* hDecoder, NeAACDecFrameInfo* hInfo, bitfile* ld, program_config* pce, drc_info* drc);
#endif
uint32_t faad_latm_frame(latm_header *latm, bitfile *ld);
#ifdef SSR_DEC
void ssr_decode(ssr_info* ssr, fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* freq_in, real_t* time_out, real_t* overlap,
                real_t ipqf_buffer[SSR_BANDS][96 / 4], real_t* prev_fmd, uint16_t frame_len);
ssr_gain_control(ssr_info* ssr, real_t* data, real_t* output, real_t* overlap, real_t* prev_fmd, uint8_t band, uint8_t window_sequence, uint16_t frame_len);
ssr_gc_function(ssr_info* ssr, real_t* prev_fmd, real_t* gc_function, uint8_t window_sequence, uint16_t frame_len);
#endif
void extract_envelope_data(sbr_info *sbr, uint8_t ch);
void extract_noise_floor_data(sbr_info *sbr, uint8_t ch);
#ifndef FIXED_POINT
void envelope_noise_dequantisation(sbr_info *sbr, uint8_t ch);
void unmap_envelope_noise(sbr_info *sbr);
#endif
void ssr_ipqf(ssr_info* ssr, real_t* in_data, real_t* out_data, real_t buffer[SSR_BANDS][96 / 4], uint16_t frame_len, uint8_t bands);
mdct_info *faad_mdct_init(uint16_t N);
void faad_mdct_end(mdct_info *mdct);
void faad_imdct(mdct_info *mdct, real_t *X_in, real_t *X_out);
void faad_mdct(mdct_info *mdct, real_t *X_in, real_t *X_out);
#if (defined(PS_DEC) || defined(DRM_PS))
uint8_t sbrDecodeSingleFramePS(sbr_info* sbr, real_t* left_channel, real_t* right_channel, const uint8_t just_seeked, const uint8_t downSampledSBR);
#endif
void unmap_envelope_noise(sbr_info* sbr);
int16_t real_to_int16(real_t sig_in);
uint8_t sbr_save_prev_data(sbr_info* sbr, uint8_t ch);
void    sbr_save_matrix(sbr_info* sbr, uint8_t ch);
fb_info *ssr_filter_bank_init(uint16_t frame_len);
void ssr_filter_bank_end(fb_info *fb);
void ssr_ifilter_bank(fb_info* fb, uint8_t window_sequence, uint8_t window_shape, uint8_t window_shape_prev, real_t* freq_in, real_t* time_out, uint16_t frame_len);
int32_t find_bands(uint8_t warp, uint8_t bands, uint8_t a0, uint8_t a1);
void     sbr_header(bitfile* ld, sbr_info* sbr);
uint8_t  calc_sbr_tables(sbr_info* sbr, uint8_t start_freq, uint8_t stop_freq, uint8_t samplerate_mode, uint8_t freq_scale, uint8_t alter_scale, uint8_t xover_band);
uint8_t  sbr_data(bitfile* ld, sbr_info* sbr);
uint16_t sbr_extension(bitfile* ld, sbr_info* sbr, uint8_t bs_extension_id, uint16_t num_bits_left);
uint8_t  sbr_single_channel_element(bitfile* ld, sbr_info* sbr);
uint8_t  sbr_channel_pair_element(bitfile* ld, sbr_info* sbr);
uint8_t  sbr_grid(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     sbr_dtdf(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     invf_mode(bitfile* ld, sbr_info* sbr, uint8_t ch);
void     sinusoidal_coding(bitfile* ld, sbr_info* sbr, uint8_t ch);
uint8_t hf_adjustment(sbr_info *sbr, qmf_t Xsbr[MAX_NTSRHFG][64], real_t *deg, uint8_t ch);
uint8_t qmf_start_channel(uint8_t bs_start_freq, uint8_t bs_samplerate_mode, uint32_t sample_rate);
uint8_t qmf_stop_channel(uint8_t bs_stop_freq, uint32_t sample_rate, uint8_t k0);
uint8_t master_frequency_table_fs0(sbr_info* sbr, uint8_t k0, uint8_t k2, uint8_t bs_alter_scale);
uint8_t master_frequency_table(sbr_info* sbr, uint8_t k0, uint8_t k2, uint8_t bs_freq_scale, uint8_t bs_alter_scale);
uint8_t derived_frequency_table(sbr_info* sbr, uint8_t bs_xover_band, uint8_t k2);
void    limiter_frequency_table(sbr_info* sbr);
#ifdef SBR_DEC
    #ifdef SBR_LOW_POWER
void calc_prediction_coef_lp(sbr_info* sbr, qmf_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, real_t* rxx);
void calc_aliasing_degree(sbr_info* sbr, real_t* rxx, real_t* deg);
    #else  // SBR_LOW_POWER
void calc_prediction_coef(sbr_info* sbr, qmf_t Xlow[MAX_NTSRHFG][64], complex_t* alpha_0, complex_t* alpha_1, uint8_t k);
    #endif // SBR_LOW_POWER
void calc_chirp_factors(sbr_info* sbr, uint8_t ch);
void patch_construction(sbr_info* sbr);
#endif // SBR_DEC
#ifdef SBR_DEC
uint8_t estimate_current_envelope(sbr_info* sbr, sbr_hfadj_info* adj, qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
void    calculate_gain(sbr_info* sbr, sbr_hfadj_info* adj, uint8_t ch);
    #ifdef SBR_LOW_POWER
void calc_gain_groups(sbr_info* sbr, sbr_hfadj_info* adj, real_t* deg, uint8_t ch);
void aliasing_reduction(sbr_info* sbr, sbr_hfadj_info* adj, real_t* deg, uint8_t ch);
    #endif // SBR_LOW_POWER
void hf_assembly(sbr_info* sbr, sbr_hfadj_info* adj, qmf_t Xsbr[MAX_NTSRHFG][64], uint8_t ch);
#endif // SBR_DEC
uint8_t get_S_mapped(sbr_info* sbr, uint8_t ch, uint8_t l, uint8_t current_band);
qmfa_info* qmfa_init(uint8_t channels);
void       qmfa_end(qmfa_info* qmfa);
qmfs_info* qmfs_init(uint8_t channels);
void       qmfs_end(qmfs_info* qmfs);
void sbr_qmf_analysis_32(sbr_info* sbr, qmfa_info* qmfa, const real_t* input, qmf_t X[MAX_NTSRHFG][64], uint8_t offset, uint8_t kx);
void sbr_qmf_synthesis_32(sbr_info* sbr, qmfs_info* qmfs, qmf_t X[MAX_NTSRHFG][64], real_t* output);
void sbr_qmf_synthesis_64(sbr_info* sbr, qmfs_info* qmfs, qmf_t X[MAX_NTSRHFG][64], real_t* output);
uint8_t envelope_time_border_vector(sbr_info *sbr, uint8_t ch);
void noise_floor_time_border_vector(sbr_info *sbr, uint8_t ch);
void hf_generation(sbr_info *sbr, qmf_t Xlow[MAX_NTSRHFG][64], qmf_t Xhigh[MAX_NTSRHFG][64], real_t *deg, uint8_t ch);
void sbr_envelope(bitfile *ld, sbr_info *sbr, uint8_t ch);
void sbr_noise(bitfile *ld, sbr_info *sbr, uint8_t ch);
uint8_t middleBorder(sbr_info* sbr, uint8_t ch);
#ifdef SSR_DEC
static real_t **pp_q0, **pp_t0, **pp_t1;
void ssr_ipqf(ssr_info* ssr, real_t* in_data, real_t* out_data, real_t buffer[SSR_BANDS][96 / 4], uint16_t frame_len, uint8_t bands);
#endif
