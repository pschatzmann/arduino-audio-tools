/*******************************************************************************************************************************************************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved. Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the
  distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the names of specific contributors, may be used to endorse or promote products derived from this software without specific prior written
  permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************************************************************************************************************************************/

#pragma once

#include "celt.h"
#include "silk.h"
#include <stdint.h>

#define SILK_MAX_FRAMES_PER_PACKET 3
/* Decoder API flags */
#define FLAG_DECODE_NORMAL 0
#define FLAG_PACKET_LOST   1
#define FLAG_DECODE_LBRR   2
/* Number of binary divisions, when not in low complexity mode */
#define BIN_DIV_STEPS_A2NLSF_FIX  3 /* must be no higher than 16 - log2( LSF_COS_TAB_SZ_FIX ) */
#define MAX_ITERATIONS_A2NLSF_FIX 16
/* Fixed point macros */
/* (a32 * b32) output have to be 32bit int */
#define silk_MUL(a32, b32) ((a32) * (b32))

/* (a32 * b32) output have to be 32bit uint */
#define silk_MUL_uint(a32, b32) silk_MUL(a32, b32)

/* a32 + (b32 * c32) output have to be 32bit int */
#define silk_MLA(a32, b32, c32) silk_ADD32((a32), ((b32) * (c32)))

/* a32 + (b32 * c32) output have to be 32bit uint */
#define silk_MLA_uint(a32, b32, c32) silk_MLA(a32, b32, c32)

/* ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */
#define silk_SMULTT(a32, b32) (((a32) >> 16) * ((b32) >> 16))

/* a32 + ((a32 >> 16)  * (b32 >> 16)) output have to be 32bit int */
#define silk_SMLATT(a32, b32, c32) silk_ADD32((a32), ((b32) >> 16) * ((c32) >> 16))

#define silk_SMLALBB(a64, b16, c16) silk_ADD64((a64), (int64_t)((int32_t)(b16) * (int32_t)(c16)))

/* (a32 * b32) */
#define silk_SMULL(a32, b32) ((int64_t)(a32) * /*(int64_t)*/ (b32))

/* Adds two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour
   (just standard two's complement implementation-specific behaviour) */
#define silk_ADD32_ovflw(a, b) ((int32_t)((uint32_t)(a) + (uint32_t)(b)))
/* Subtractss two signed 32-bit values in a way that can overflow, while not relying on undefined behaviour
   (just standard two's complement implementation-specific behaviour) */
#define silk_SUB32_ovflw(a, b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)))

/* Multiply-accumulate macros that allow overflow in the addition (ie, no asserts in debug mode) */
#define silk_MLA_ovflw(a32, b32, c32)    silk_ADD32_ovflw((a32), (uint32_t)(b32) * (uint32_t)(c32))
#define silk_SMLABB_ovflw(a32, b32, c32) (silk_ADD32_ovflw((a32), ((int32_t)((int16_t)(b32))) * (int32_t)((int16_t)(c32))))

#define silk_DIV32_16(a32, b16) ((int32_t)((a32) / (b16)))
#define silk_DIV32(a32, b32)    ((int32_t)((a32) / (b32)))

/* These macros enables checking for overflow in silk_API_Debug.h*/
#define silk_ADD16(a, b) ((a) + (b))
#define silk_ADD32(a, b) ((a) + (b))
#define silk_ADD64(a, b) ((a) + (b))

#define silk_SUB16(a, b) ((a) - (b))
#define silk_SUB32(a, b) ((a) - (b))
#define silk_SUB64(a, b) ((a) - (b))

#define silk_SAT8(a)  ((a) > silk_int8_MAX ? silk_int8_MAX : ((a) < silk_int8_MIN ? silk_int8_MIN : (a)))
#define silk_SAT16(a) ((a) > silk_int16_MAX ? silk_int16_MAX : ((a) < silk_int16_MIN ? silk_int16_MIN : (a)))
#define silk_SAT32(a) ((a) > silk_int32_MAX ? silk_int32_MAX : ((a) < silk_int32_MIN ? silk_int32_MIN : (a)))

#define silk_CHECK_FIT8(a)  (a)
#define silk_CHECK_FIT16(a) (a)
#define silk_CHECK_FIT32(a) (a)

#define silk_ADD_SAT16(a, b) (int16_t) silk_SAT16(silk_ADD32((int32_t)(a), (b)))
#define silk_ADD_SAT64(a, b) \
    ((((a) + (b)) & 0x8000000000000000LL) == 0 ? ((((a) & (b)) & 0x8000000000000000LL) != 0 ? silk_int64_MIN : (a) + (b)) : ((((a) | (b)) & 0x8000000000000000LL) == 0 ? silk_int64_MAX : (a) + (b)))

#define silk_SUB_SAT16(a, b) (int16_t) silk_SAT16(silk_SUB32((int32_t)(a), (b)))
#define silk_SUB_SAT64(a, b)                                                                                                                \
    ((((a) - (b)) & 0x8000000000000000LL) == 0 ? (((a) & ((b) ^ 0x8000000000000000LL) & 0x8000000000000000LL) ? silk_int64_MIN : (a) - (b)) \
                                               : ((((a) ^ 0x8000000000000000LL) & (b) & 0x8000000000000000LL) ? silk_int64_MAX : (a) - (b)))

/* Saturation for positive input values */
#define silk_POS_SAT32(a) ((a) > silk_int32_MAX ? silk_int32_MAX : (a))

/* Add with saturation for positive input values */
#define silk_ADD_POS_SAT8(a, b)  ((((a) + (b)) & 0x80) ? silk_int8_MAX : ((a) + (b)))
#define silk_ADD_POS_SAT16(a, b) ((((a) + (b)) & 0x8000) ? silk_int16_MAX : ((a) + (b)))
#define silk_ADD_POS_SAT32(a, b) ((((uint32_t)(a) + (uint32_t)(b)) & 0x80000000) ? silk_int32_MAX : ((a) + (b)))

#define silk_LSHIFT8(a, shift)  ((int32_t8)((uint8_t)(a) << (shift))) /* shift >= 0, shift < 8  */
#define silk_LSHIFT16(a, shift) ((int16_t)((uint16_t)(a) << (shift))) /* shift >= 0, shift < 16 */
#define silk_LSHIFT32(a, shift) ((int32_t)((uint32_t)(a) << (shift))) /* shift >= 0, shift < 32 */
#define silk_LSHIFT64(a, shift) ((int64_t)((uint64_t)(a) << (shift))) /* shift >= 0, shift < 64 */
#define silk_LSHIFT(a, shift)   silk_LSHIFT32(a, shift)               /* shift >= 0, shift < 32 */

#define silk_RSHIFT8(a, shift)  ((a) >> (shift))        /* shift >= 0, shift < 8  */
#define silk_RSHIFT16(a, shift) ((a) >> (shift))        /* shift >= 0, shift < 16 */
#define silk_RSHIFT32(a, shift) ((a) >> (shift))        /* shift >= 0, shift < 32 */
#define silk_RSHIFT64(a, shift) ((a) >> (shift))        /* shift >= 0, shift < 64 */
#define silk_RSHIFT(a, shift)   silk_RSHIFT32(a, shift) /* shift >= 0, shift < 32 */

/* saturates before shifting */
#define silk_LSHIFT_SAT32(a, shift) (silk_LSHIFT32(silk_LIMIT((a), silk_RSHIFT32(silk_int32_MIN, (shift)), silk_RSHIFT32(silk_int32_MAX, (shift))), (shift)))

#define silk_LSHIFT_ovflw(a, shift) ((int32_t)((uint32_t)(a) << (shift))) /* shift >= 0, allowed to overflow */
#define silk_LSHIFT_uint(a, shift)  ((a) << (shift))                      /* shift >= 0 */
#define silk_RSHIFT_uint(a, shift)  ((a) >> (shift))                      /* shift >= 0 */

#define silk_ADD_LSHIFT(a, b, shift)      ((a) + silk_LSHIFT((b), (shift)))            /* shift >= 0 */
#define silk_ADD_LSHIFT32(a, b, shift)    silk_ADD32((a), silk_LSHIFT32((b), (shift))) /* shift >= 0 */
#define silk_ADD_LSHIFT_uint(a, b, shift) ((a) + silk_LSHIFT_uint((b), (shift)))       /* shift >= 0 */
#define silk_ADD_RSHIFT(a, b, shift)      ((a) + silk_RSHIFT((b), (shift)))            /* shift >= 0 */
#define silk_ADD_RSHIFT32(a, b, shift)    silk_ADD32((a), silk_RSHIFT32((b), (shift))) /* shift >= 0 */
#define silk_ADD_RSHIFT_uint(a, b, shift) ((a) + silk_RSHIFT_uint((b), (shift)))       /* shift >= 0 */
#define silk_SUB_LSHIFT32(a, b, shift)    silk_SUB32((a), silk_LSHIFT32((b), (shift))) /* shift >= 0 */
#define silk_SUB_RSHIFT32(a, b, shift)    silk_SUB32((a), silk_RSHIFT32((b), (shift))) /* shift >= 0 */

/* Requires that shift > 0 */
#define silk_RSHIFT_ROUND(a, shift)   ((shift) == 1 ? ((a) >> 1) + ((a) & 1) : (((a) >> ((shift)-1)) + 1) >> 1)
#define silk_RSHIFT_ROUND64(a, shift) ((shift) == 1 ? ((a) >> 1) + ((a) & 1) : (((a) >> ((shift)-1)) + 1) >> 1)

/* Number of rightshift required to fit the multiplication */
#define silk_NSHIFT_MUL_32_32(a, b) (-(31 - (32 - silk_CLZ32(silk_abs(a)) + (32 - silk_CLZ32(silk_abs(b))))))
#define silk_NSHIFT_MUL_16_16(a, b) (-(15 - (16 - silk_CLZ16(silk_abs(a)) + (16 - silk_CLZ16(silk_abs(b))))))

#define silk_min(a, b) (((a) < (b)) ? (a) : (b))
#define silk_max(a, b) (((a) > (b)) ? (a) : (b))

#define MIN_QGAIN_DB                              2  /* dB level of lowest gain quantization level */
#define MAX_QGAIN_DB                              88 /* dB level of highest gain quantization level */
#define N_LEVELS_QGAIN                            64 /* Number of gain quantization levels */
#define MAX_DELTA_GAIN_QUANT                      36 /* Max increase in gain quantization index */
#define MIN_DELTA_GAIN_QUANT                      -4 /* Max decrease in gain quantization index */
#define OFFSET_VL_Q10                             32 /* Quantization offsets (multiples of 4) */
#define OFFSET_VH_Q10                             100
#define OFFSET_UVL_Q10                            100
#define OFFSET_UVH_Q10                            240
#define QUANT_LEVEL_ADJUST_Q10                    80
#define MAX_LPC_STABILIZE_ITERATIONS              16 /* Maximum numbers of iterations used to stabilize an LPC vector */
#define MAX_PREDICTION_POWER_GAIN                 1e4f
#define MAX_PREDICTION_POWER_GAIN_AFTER_RESET     1e2f
#define MAX_LPC_ORDER                             16
#define MIN_LPC_ORDER                             10
#define LTP_ORDER                                 5  /* Find Pred Coef defines */
#define NB_LTP_CBKS                               3  /* LTP quantization settings */
#define USE_HARM_SHAPING                          1  /* Flag to use harmonic noise shaping */
#define MAX_SHAPE_LPC_ORDER                       24 /* Max LPC order of noise shaping filters */
#define MAX_DEL_DEC_STATES                        4 /* Maximum number of delayed decision states */
#define LTP_BUF_LENGTH                            512
#define LTP_MASK                                  (LTP_BUF_LENGTH - 1)
#define DECISION_DELAY                            40
#define MAX_NB_SUBFR                              4 /* Maximum number of subframes */
#define DECODER_NUM_CHANNELS                      2 /* Number of decoder channels (1/2) */
#define MAX_FRAMES_PER_PACKET                     3
#define MIN_TARGET_RATE_BPS                       5000 /* Limits on bitrate */
#define MAX_TARGET_RATE_BPS                       80000
#define LBRR_NB_MIN_RATE_BPS                      12000 /* LBRR thresholds */
#define LBRR_MB_MIN_RATE_BPS                      14000
#define LBRR_WB_MIN_RATE_BPS                      16000
#define NB_SPEECH_FRAMES_BEFORE_DTX               10 /* eq 200 ms */
#define MAX_CONSECUTIVE_DTX                       20 /* eq 400 ms */
#define DTX_ACTIVITY_THRESHOLD                    0.1f
#define VAD_NO_DECISION                           -1 /* VAD decision */
#define VAD_NO_ACTIVITY                           0
#define VAD_ACTIVITY                              1
#define MAX_FS_KHZ                                16 /* Maximum sampling frequency */
#define MAX_API_FS_KHZ                            48
#define TYPE_NO_VOICE_ACTIVITY                    0 /* Signal types */
#define TYPE_UNVOICED                             1
#define TYPE_VOICED                               2
#define CODE_INDEPENDENTLY                        0 /* Conditional coding types */
#define CODE_INDEPENDENTLY_NO_LTP_SCALING         1
#define CODE_CONDITIONALLY                        2
#define STEREO_QUANT_TAB_SIZE                     16 /* Settings for stereo processing */
#define STEREO_QUANT_SUB_STEPS                    5
#define STEREO_INTERP_LEN_MS                      8    /* must be even */
#define STEREO_RATIO_SMOOTH_COEF                  0.01 /* smoothing coef for signal norms and stereo width */
#define PITCH_EST_MIN_LAG_MS                      2    /* 2 ms -> 500 Hz */
#define PITCH_EST_MAX_LAG_MS                      18   /* 18 ms -> 56 Hz */
#define LTP_MEM_LENGTH_MS                         20   /* Number of samples per frame */
#define SUB_FRAME_LENGTH_MS                       5
#define MAX_SUB_FRAME_LENGTH                      (SUB_FRAME_LENGTH_MS * MAX_FS_KHZ)
#define MAX_FRAME_LENGTH_MS                       (SUB_FRAME_LENGTH_MS * MAX_NB_SUBFR)
#define MAX_FRAME_LENGTH                          (MAX_FRAME_LENGTH_MS * MAX_FS_KHZ)
#define LA_PITCH_MS                               2 /* Milliseconds of lookahead for pitch analysis */
#define LA_PITCH_MAX                              (LA_PITCH_MS * MAX_FS_KHZ)
#define MAX_FIND_PITCH_LPC_ORDER                  16                        /* Order of LPC used in find pitch */
#define FIND_PITCH_LPC_WIN_MS                     (20 + (LA_PITCH_MS << 1)) /* Length of LPC window used in find pitch */
#define FIND_PITCH_LPC_WIN_MS_2_SF                (10 + (LA_PITCH_MS << 1))
#define FIND_PITCH_LPC_WIN_MAX                    (FIND_PITCH_LPC_WIN_MS * MAX_FS_KHZ)
#define LA_SHAPE_MS                               5 /* Milliseconds of lookahead for noise shape analysis */
#define LA_SHAPE_MAX                              (LA_SHAPE_MS * MAX_FS_KHZ)
#define SHAPE_LPC_WIN_MAX                         (15 * MAX_FS_KHZ) /* Max lenof LPCwindow in noise shape analysis */
#define SHELL_CODEC_FRAME_LENGTH                  16                /* Number of subframes for excitation entropy coding */
#define LOG2_SHELL_CODEC_FRAME_LENGTH             4
#define MAX_NB_SHELL_BLOCKS                       (MAX_FRAME_LENGTH / SHELL_CODEC_FRAME_LENGTH)
#define N_RATE_LEVELS                             10            /* Number of rate levels, for entropy coding of excitation */
#define SILK_MAX_PULSES                           16            /* Maximum sum of pulses per shell coding frame */
#define MAX_MATRIX_SIZE                           MAX_LPC_ORDER /* Max of LPC Order and LTP order */
#define NSQ_LPC_BUF_LENGTH                        MAX_LPC_ORDER
#define VAD_N_BANDS                               4
#define VAD_INTERNAL_SUBFRAMES_LOG2               2
#define VAD_INTERNAL_SUBFRAMES                    (1 << VAD_INTERNAL_SUBFRAMES_LOG2)
#define VAD_NOISE_LEVEL_SMOOTH_COEF_Q16           1024 /* Must be <  4096 */
#define VAD_NOISE_LEVELS_BIAS                     50
#define VAD_NEGATIVE_OFFSET_Q5                    128 /* sigmoid is 0 at -128 */
#define VAD_SNR_FACTOR_Q16                        45000
#define VAD_SNR_SMOOTH_COEF_Q18                   4096 /* smoothing for SNR measurement */
#define LSF_COS_TAB_SZ_FIX                        128  /* Sizeof piecewise linear cos approximation table for the LSFs */
#define BWE_COEF                                  0.99
#define V_PITCH_GAIN_START_MIN_Q14                11469
#define V_PITCH_GAIN_START_MAX_Q14                15565
#define MAX_PITCH_LAG_MS                          18
#define RAND_BUF_SIZE                             128
#define RAND_BUF_MASK                             (RAND_BUF_SIZE - 1)
#define LOG2_INV_LPC_GAIN_HIGH_THRES              3
#define LOG2_INV_LPC_GAIN_LOW_THRES               8
#define PITCH_DRIFT_FAC_Q16                       655
#define BITRESERVOIR_DECAY_TIME_MS                500    /* Decay time for bitreservoir */
#define FIND_PITCH_WHITE_NOISE_FRACTION           1e-3f  /* Level of noise floor for whitening filter LPC analysis */
#define FIND_PITCH_BANDWIDTH_EXPANSION            0.99f  /* Bandwidth expansion for whitening filter in pitch analysis */
#define FIND_LPC_COND_FAC                         1e-5f  /* LPC analysis regularization */
#define MAX_SUM_LOG_GAIN_DB                       250.0f /* Max cumulative LTP gain */
#define LTP_CORR_INV_MAX                          0.03f  /* LTP analysis defines */
#define VARIABLE_HP_SMTH_COEF1                    0.1f
#define VARIABLE_HP_SMTH_COEF2                    0.015f
#define VARIABLE_HP_MAX_DELTA_FREQ                0.4f
#define VARIABLE_HP_MIN_CUTOFF_HZ                 60 /* Min and max cut-off frequency values (-3 dB points) */
#define VARIABLE_HP_MAX_CUTOFF_HZ                 100
#define SPEECH_ACTIVITY_DTX_THRES                 0.05f /* VAD threshold */
#define LBRR_SPEECH_ACTIVITY_THRES                0.3f  /* Speech Activity LBRR enable threshold */
#define BG_SNR_DECR_dB                            2.0f  /* reduction in coding SNR during low speech activity */
#define HARM_SNR_INCR_dB                          2.0f  /* factor for reducing quantization noise during voiced speech */
#define SPARSE_SNR_INCR_dB                        2.0f  /* factor for reducing quant. noise for unvoiced sparse signals */
#define ENERGY_VARIATION_THRESHOLD_QNT_OFFSET     0.6f
#define WARPING_MULTIPLIER                        0.015f /* warping control */
#define SHAPE_WHITE_NOISE_FRACTION                3e-5f  /* fraction added to first autocorrelation value */
#define BANDWIDTH_EXPANSION                       0.94f  /* noise shaping filter chirp factor */
#define HARMONIC_SHAPING                          0.3f   /* harmonic noise shaping */
#define HIGH_RATE_OR_LOW_QUALITY_HARMONIC_SHAPING 0.2f   /* extra harmonic noise shaping for high bitr. or noisy input */
#define HP_NOISE_COEF                             0.25f  /* parameter for shaping noise towards higher frequencies */
#define HARM_HP_NOISE_COEF                        0.35f
#define INPUT_TILT                                0.05f
#define HIGH_RATE_INPUT_TILT                      0.1f /* for extra high-pass tilt to the input signal at high rates */
#define LOW_FREQ_SHAPING                          4.0f /* parameter for reducing noise at the very low frequencies */
#define LOW_QUALITY_LOW_FREQ_SHAPING_DECR         0.5f
#define SUBFR_SMTH_COEF                           0.4f
#define LAMBDA_OFFSET                             1.2f /* param. defining the R/D tradeoff in the residual quantizer */
#define LAMBDA_SPEECH_ACT                         -0.2f
#define LAMBDA_DELAYED_DECISIONS                  -0.05f
#define LAMBDA_INPUT_QUALITY                      -0.1f
#define LAMBDA_CODING_QUALITY                     -0.2f
#define LAMBDA_QUANT_OFFSET                       0.8f
#define REDUCE_BITRATE_10_MS_BPS                  2200 /* Compensation in bitrate calculations for 10 ms modes */
#define MAX_BANDWIDTH_SWITCH_DELAY_MS             5000 /* Maximum time before allowing a bandwidth transition */

#define silk_int64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFLL) /*  2^63 - 1 */
#define silk_int64_MIN ((int64_t)0x8000000000000000LL) /* -2^63 */
#define silk_int32_MAX 0x7FFFFFFF                      /*  2^31 - 1 =  2147483647 */
#define silk_int32_MIN ((int32_t)0x80000000)           /* -2^31     = -2147483648 */
#define silk_int16_MAX 0x7FFF                          /*  2^15 - 1 =  32767 */
#define silk_int16_MIN ((int16_t)0x8000)               /* -2^15     = -32768 */
#define silk_int8_MAX  0x7F                            /*  2^7 - 1  =  127 */
#define silk_int8_MIN  ((int8_t)0x80)                  /* -2^7      = -128 */
#define silk_uint8_MAX 0xFF                            /*  2^8 - 1 = 255 */

#define silk_TRUE  1
#define silk_FALSE 0

#define VARDECL(type, var)
#define ALLOC(var, size, type) type var[size]
#define silk_enc_map(a)        (silk_RSHIFT((a), 15) + 1)
#define silk_dec_map(a)        (silk_LSHIFT((a), 1) - 1)

/* Macro to convert floating-point constants to fixed-point */
#define SILK_FIX_CONST(C, Q) ((int32_t)((C) * ((int64_t)1 << (Q)) + 0.5L))

/* define macros as empty strings */
#define TIC(TAG_NAME)
#define TOC(TAG_NAME)
#define silk_TimerSave(FILE_NAME)

/* NLSF quantizer */
#define NLSF_W_Q                       2
#define NLSF_VQ_MAX_VECTORS            32
#define NLSF_QUANT_MAX_AMPLITUDE       4
#define NLSF_QUANT_MAX_AMPLITUDE_EXT   10
#define NLSF_QUANT_LEVEL_ADJ           0.1
#define NLSF_QUANT_DEL_DEC_STATES_LOG2 2
#define NLSF_QUANT_DEL_DEC_STATES      (1 << NLSF_QUANT_DEL_DEC_STATES_LOG2)

/* Transition filtering for mode switching */
#define TRANSITION_TIME_MS   5120 /* 5120 = 64 * FRAME_LENGTH_MS * ( TRANSITION_INT_NUM - 1 ) = 64*(20*4)*/
#define TRANSITION_NB        3    /* Hardcoded in tables */
#define TRANSITION_NA        2    /* Hardcoded in tables */
#define TRANSITION_INT_NUM   5    /* Hardcoded in tables */
#define TRANSITION_FRAMES    (TRANSITION_TIME_MS / MAX_FRAME_LENGTH_MS)
#define TRANSITION_INT_STEPS (TRANSITION_FRAMES / (TRANSITION_INT_NUM - 1))

/* BWE factors to apply after packet loss */
#define BWE_AFTER_LOSS_Q16 63570

/* Defines for CN generation */
#define CNG_BUF_MASK_MAX  255   /* 2^floor(log2(MAX_FRAME_LENGTH))-1    */
#define CNG_GAIN_SMTH_Q16 4634  /* 0.25^(1/4)                           */
#define CNG_NLSF_SMTH_Q16 16348 /* 0.25                                 */
#define PE_MAX_FS_KHZ     16    /* Maximum sampling frequency used */

#define PE_MAX_NB_SUBFR    4
#define PE_SUBFR_LENGTH_MS 5 /* 5 ms */

#define PE_LTP_MEM_LENGTH_MS (4 * PE_SUBFR_LENGTH_MS)

#define PE_MAX_FRAME_LENGTH_MS   (PE_LTP_MEM_LENGTH_MS + PE_MAX_NB_SUBFR * PE_SUBFR_LENGTH_MS)
#define PE_MAX_FRAME_LENGTH      (PE_MAX_FRAME_LENGTH_MS * PE_MAX_FS_KHZ)
#define PE_MAX_FRAME_LENGTH_ST_1 (PE_MAX_FRAME_LENGTH >> 2)
#define PE_MAX_FRAME_LENGTH_ST_2 (PE_MAX_FRAME_LENGTH >> 1)

#define PE_MAX_LAG_MS             18 /* 18 ms -> 56 Hz */
#define PE_MIN_LAG_MS             2  /* 2 ms -> 500 Hz */
#define PE_MAX_LAG                (PE_MAX_LAG_MS * PE_MAX_FS_KHZ)
#define PE_MIN_LAG                (PE_MIN_LAG_MS * PE_MAX_FS_KHZ)
#define PE_D_SRCH_LENGTH          24
#define PE_NB_STAGE3_LAGS         5
#define PE_NB_CBKS_STAGE2         3
#define PE_NB_CBKS_STAGE2_EXT     11
#define PE_NB_CBKS_STAGE3_MAX     34
#define PE_NB_CBKS_STAGE3_MID     24
#define PE_NB_CBKS_STAGE3_MIN     16
#define PE_NB_CBKS_STAGE3_10MS    12
#define PE_NB_CBKS_STAGE2_10MS    3
#define PE_SHORTLAG_BIAS          0.2f /* for logarithmic weighting    */
#define PE_PREVLAG_BIAS           0.2f /* for logarithmic weighting    */
#define PE_FLATCONTOUR_BIAS       0.05f
#define SILK_PE_MIN_COMPLEX       0
#define SILK_PE_MID_COMPLEX       1
#define SILK_PE_MAX_COMPLEX       2
#define USE_CELT_FIR              0
#define MAX_LOOPS                 20
#define NB_ATT                    2
#define ORDER_FIR                 4
#define RESAMPLER_DOWN_ORDER_FIR0 18
#define RESAMPLER_DOWN_ORDER_FIR1 24
#define RESAMPLER_DOWN_ORDER_FIR2 36
#define RESAMPLER_ORDER_FIR_12    8
#define SILK_MAX_ORDER_LPC        24 /* max order of the LPC analysis in schur() and k2a() */

#define SILK_RESAMPLER_MAX_FIR_ORDER 36
#define SILK_RESAMPLER_MAX_IIR_ORDER 6
#define A_LIMIT                      SILK_FIX_CONST(0.99975, 24)
#define MUL32_FRAC_Q(a32, b32, Q)    ((int32_t)(silk_RSHIFT_ROUND64(silk_SMULL(a32, b32), Q)))
/* Number of input samples to process in the inner loop */
#define RESAMPLER_MAX_BATCH_SIZE_MS 10
#define RESAMPLER_MAX_FS_KHZ        48
#define RESAMPLER_MAX_BATCH_SIZE_IN (RESAMPLER_MAX_BATCH_SIZE_MS * RESAMPLER_MAX_FS_KHZ)

/* Simple way to make [8000, 12000, 16000, 24000, 48000] to [0, 1, 2, 3, 4] */
#define rateID(R) (((((R) >> 12) - ((R) > 16000)) >> ((R) > 24000)) - 1)

#define USE_silk_resampler_copy                   (0)
#define USE_silk_resampler_private_up2_HQ_wrapper (1)
#define USE_silk_resampler_private_IIR_FIR        (2)
#define USE_silk_resampler_private_down_FIR       (3)

/* Decoder error messages */
#define SILK_NO_ERROR                       0
#define SILK_DEC_INVALID_SAMPLING_FREQUENCY -200 /* Output samplfreq lower than intern. decoded sampling freq */
#define SILK_DEC_PAYLOAD_TOO_LARGE          -201 /* Payload size exceeded the maximum allowed 1024 bytes */
#define SILK_DEC_PAYLOAD_ERROR              -202 /* Payload has bit errors */
#define SILK_DEC_INVALID_FRAME_SIZE         -203 /* Payload has bit errors */

#define silk_LIMIT(a, limit1, limit2) ((limit1) > (limit2) ? ((a) > (limit1) ? (limit1) : ((a) < (limit2) ? (limit2) : (a))) : ((a) > (limit2) ? (limit2) : ((a) < (limit1) ? (limit1) : (a))))

#define silk_sign(a) ((a) > 0 ? 1 : ((a) < 0 ? -1 : 0))

#define silk_LIMIT_int silk_LIMIT
#define silk_LIMIT_16  silk_LIMIT
#define silk_LIMIT_32  silk_LIMIT

#define silk_abs(a)       (((a) > 0) ? (a) : -(a))
#define silk_abs_int(a)   (((a) ^ ((a) >> (8 * sizeof(a) - 1))) - ((a) >> (8 * sizeof(a) - 1)))
#define silk_abs_int32(a) (((a) ^ ((a) >> 31)) - ((a) >> 31))
#define silk_abs_int64(a) (((a) > 0) ? (a) : -(a))

#define OFFSET        ((MIN_QGAIN_DB * 128) / 6 + 16 * 128)
#define SCALE_Q16     ((65536 * (N_LEVELS_QGAIN - 1)) / (((MAX_QGAIN_DB - MIN_QGAIN_DB) * 128) / 6))
#define INV_SCALE_Q16 ((65536 * (((MAX_QGAIN_DB - MIN_QGAIN_DB) * 128) / 6)) / (N_LEVELS_QGAIN - 1))

/* (a32 * (int32_t)((int16_t)(b32))) >> 16 output have to be 32bit int */
#define silk_SMULWB(a32, b32) ((int32_t)(((a32) * (int64_t)((int16_t)(b32))) >> 16))

/* a32 + (b32 * (int32_t)((int16_t)(c32))) >> 16 output have to be 32bit int */
#define silk_SMLAWB(a32, b32, c32) ((int32_t)((a32) + (((b32) * (int64_t)((int16_t)(c32))) >> 16)))

/* (a32 * (b32 >> 16)) >> 16 */
#define silk_SMULWT(a32, b32) ((int32_t)(((a32) * (int64_t)((b32) >> 16)) >> 16))

/* a32 + (b32 * (c32 >> 16)) >> 16 */
#define silk_SMLAWT(a32, b32, c32) ((int32_t)((a32) + (((b32) * ((int64_t)(c32) >> 16)) >> 16)))

/* (int32_t)((int16_t)(a3))) * (int32_t)((int16_t)(b32)) output have to be 32bit int */
#define silk_SMULBB(a32, b32) ((int32_t)((int16_t)(a32)) * (int32_t)((int16_t)(b32)))

/* a32 + (int32_t)((int16_t)(b32)) * (int32_t)((int16_t)(c32)) output have to be 32bit int */
#define silk_SMLABB(a32, b32, c32) ((a32) + ((int32_t)((int16_t)(b32))) * (int32_t)((int16_t)(c32)))

/* (int32_t)((int16_t)(a32)) * (b32 >> 16) */
#define silk_SMULBT(a32, b32) ((int32_t)((int16_t)(a32)) * ((b32) >> 16))

/* a32 + (int32_t)((int16_t)(b32)) * (c32 >> 16) */
#define silk_SMLABT(a32, b32, c32) ((a32) + ((int32_t)((int16_t)(b32))) * ((c32) >> 16))

/* a64 + (b32 * c32) */
#define silk_SMLAL(a64, b32, c32) (silk_ADD64((a64), ((int64_t)(b32) * (int64_t)(c32))))

/* (a32 * b32) >> 16 */
#define silk_SMULWW(a32, b32) ((int32_t)(((int64_t)(a32) * (b32)) >> 16))

/* a32 + ((b32 * c32) >> 16) */
#define silk_SMLAWW(a32, b32, c32) ((int32_t)((a32) + (((int64_t)(b32) * (c32)) >> 16)))

/* add/subtract with output saturated */
#define silk_ADD_SAT32(a, b) \
    ((((uint32_t)(a) + (uint32_t)(b)) & 0x80000000) == 0 ? ((((a) & (b)) & 0x80000000) != 0 ? silk_int32_MIN : (a) + (b)) : ((((a) | (b)) & 0x80000000) == 0 ? silk_int32_MAX : (a) + (b)))

#define silk_SUB_SAT32(a, b)                                                                                                      \
    ((((uint32_t)(a) - (uint32_t)(b)) & 0x80000000) == 0 ? (((a) & ((b) ^ 0x80000000) & 0x80000000) ? silk_int32_MIN : (a) - (b)) \
                                                         : ((((a) ^ 0x80000000) & (b) & 0x80000000) ? silk_int32_MAX : (a) - (b)))

static inline int32_t silk_CLZ16(int16_t in16) { return 32 - EC_ILOG(in16 << 16 | 0x8000); }

static inline int32_t silk_CLZ32(int32_t in32) { return in32 ? 32 - EC_ILOG(in32) : 32; }

/* Row based */
#define matrix_ptr(Matrix_base_adr, row, column, N) (*((Matrix_base_adr) + ((row) * (N) + (column))))
#define matrix_adr(Matrix_base_adr, row, column, N) ((Matrix_base_adr) + ((row) * (N) + (column)))

/* Column based */
#ifndef matrix_c_ptr
    #define matrix_c_ptr(Matrix_base_adr, row, column, M) (*((Matrix_base_adr) + ((row) + (M) * (column))))
#endif

#define silk_VQ_WMat_EC(ind, res_nrg_Q15, rate_dist_Q8, gain_Q7, XX_Q17, xX_Q17, cb_Q7, cb_gain_Q7, cl_Q5, subfr_len, max_gain_Q7, L) \
    (silk_VQ_WMat_EC_c(ind, res_nrg_Q15, rate_dist_Q8, gain_Q7, XX_Q17, xX_Q17, cb_Q7, cb_gain_Q7, cl_Q5, subfr_len, max_gain_Q7, L))

#define silk_NSQ(psEncC, NSQ, psIndices, x16, pulses, PredCoef_Q12, LTPCoef_Q14, AR_Q13, HarmShapeGain_Q14, Tilt_Q14, LF_shp_Q14, Gains_Q16, pitchL, Lambda_Q10, LTP_scale_Q14) \
    (silk_NSQ_c(psEncC, NSQ, psIndices, x16, pulses, PredCoef_Q12, LTPCoef_Q14, AR_Q13, HarmShapeGain_Q14, Tilt_Q14, LF_shp_Q14, Gains_Q16, pitchL, Lambda_Q10, LTP_scale_Q14))

#define silk_NSQ_del_dec(psEncC, NSQ, psIndices, x16, pulses, PredCoef_Q12, LTPCoef_Q14, AR_Q13, HarmShapeGain_Q14, Tilt_Q14, LF_shp_Q14, Gains_Q16, pitchL, Lambda_Q10, LTP_scale_Q14) \
    (silk_NSQ_del_dec_c(psEncC, NSQ, psIndices, x16, pulses, PredCoef_Q12, LTPCoef_Q14, AR_Q13, HarmShapeGain_Q14, Tilt_Q14, LF_shp_Q14, Gains_Q16, pitchL, Lambda_Q10, LTP_scale_Q14))

#define silk_VAD_GetSA_Q8(psEnC, pIn) (silk_VAD_GetSA_Q8_c(psEnC, pIn))

#define silk_noise_shape_quantizer_short_prediction(in, coef, coefRev, order) (silk_noise_shape_quantizer_short_prediction_c(in, coef, order))

#define silk_SMMUL(a32, b32) (int32_t) silk_RSHIFT64(silk_SMULL((a32), (b32)), 32)

#define silk_biquad_alt_stride2(in, B_Q28, A_Q28, S, out, len) (silk_biquad_alt_stride2_c(in, B_Q28, A_Q28, S, out, len))
#define silk_LPC_inverse_pred_gain(A_Q12, order)               (silk_LPC_inverse_pred_gain_c(A_Q12, order))

#define silk_sign(a)    ((a) > 0 ? 1 : ((a) < 0 ? -1 : 0))
#define RAND_MULTIPLIER 196314165
#define RAND_INCREMENT  907633515
#define silk_RAND(seed) (silk_MLA_ovflw((RAND_INCREMENT), (seed), (RAND_MULTIPLIER)))

#define SCRATCH_SIZE   22
#define SF_LENGTH_4KHZ (PE_SUBFR_LENGTH_MS * 4)
#define SF_LENGTH_8KHZ (PE_SUBFR_LENGTH_MS * 8)
#define MIN_LAG_4KHZ   (PE_MIN_LAG_MS * 4)
#define MIN_LAG_8KHZ   (PE_MIN_LAG_MS * 8)
#define MAX_LAG_4KHZ   (PE_MAX_LAG_MS * 4)
#define MAX_LAG_8KHZ   (PE_MAX_LAG_MS * 8 - 1)
#define CSTRIDE_4KHZ   (MAX_LAG_4KHZ + 1 - MIN_LAG_4KHZ)
#define CSTRIDE_8KHZ   (MAX_LAG_8KHZ + 3 - (MIN_LAG_8KHZ - 2))
#define D_COMP_MIN     (MIN_LAG_8KHZ - 3)
#define D_COMP_MAX     (MAX_LAG_8KHZ + 4)
#define D_COMP_STRIDE  (D_COMP_MAX - D_COMP_MIN)

typedef int32_t silk_pe_stage3_vals[PE_NB_STAGE3_LAGS];

#define MAX_FRAME_SIZE   384 /* subfr_length * nb_subfr = ( 0.005 * 16000 + 16 ) * 4 = 384 */
#define N_BITS_HEAD_ROOM 3
#define MIN_RSHIFTS      -16
#define MAX_RSHIFTS      (32 - 25) // QA_ = 25
#define QC               10
#define QS               13

static inline int32_t silk_noise_shape_quantizer_short_prediction_c(const int32_t* buf32, const int16_t* coef16, int32_t order) {
    int32_t out;
    assert(order == 10 || order == 16);

    /* Avoids introducing a bias because silk_SMLAWB() always rounds to -inf */
    out = silk_RSHIFT(order, 1);
    out = silk_SMLAWB(out, buf32[0], coef16[0]);
    out = silk_SMLAWB(out, buf32[-1], coef16[1]);
    out = silk_SMLAWB(out, buf32[-2], coef16[2]);
    out = silk_SMLAWB(out, buf32[-3], coef16[3]);
    out = silk_SMLAWB(out, buf32[-4], coef16[4]);
    out = silk_SMLAWB(out, buf32[-5], coef16[5]);
    out = silk_SMLAWB(out, buf32[-6], coef16[6]);
    out = silk_SMLAWB(out, buf32[-7], coef16[7]);
    out = silk_SMLAWB(out, buf32[-8], coef16[8]);
    out = silk_SMLAWB(out, buf32[-9], coef16[9]);

    if(order == 16) {
        out = silk_SMLAWB(out, buf32[-10], coef16[10]);
        out = silk_SMLAWB(out, buf32[-11], coef16[11]);
        out = silk_SMLAWB(out, buf32[-12], coef16[12]);
        out = silk_SMLAWB(out, buf32[-13], coef16[13]);
        out = silk_SMLAWB(out, buf32[-14], coef16[14]);
        out = silk_SMLAWB(out, buf32[-15], coef16[15]);
    }
    return out;
}

/* Struct for TOC (Table of Contents) */
typedef struct {
    int32_t VADFlag;                              /* Voice activity for packet                            */
    int32_t VADFlags[SILK_MAX_FRAMES_PER_PACKET]; /* Voice activity for each frame in packet              */
    int32_t inbandFECFlag;                        /* Flag indicating if packet contains in-band FEC       */
} silk_TOC_struct;

typedef struct {
    int8_t  GainsIndices[MAX_NB_SUBFR];
    int8_t  LTPIndex[MAX_NB_SUBFR];
    int8_t  NLSFIndices[MAX_LPC_ORDER + 1];
    int16_t lagIndex;
    int8_t  contourIndex;
    int8_t  signalType;
    int8_t  quantOffsetType;
    int8_t  NLSFInterpCoef_Q2;
    int8_t  PERIndex;
    int8_t  LTP_scaleIndex;
    int8_t  Seed;
} SideInfoIndices;

typedef struct {
    int16_t xq[2 * MAX_FRAME_LENGTH]; /* Buffer for quantized output signal                             */
    int32_t sLTP_shp_Q14[2 * MAX_FRAME_LENGTH];
    int32_t sLPC_Q14[MAX_SUB_FRAME_LENGTH + NSQ_LPC_BUF_LENGTH];
    int32_t sAR2_Q14[MAX_SHAPE_LPC_ORDER];
    int32_t sLF_AR_shp_Q14;
    int32_t sDiff_shp_Q14;
    int32_t lagPrev;
    int32_t sLTP_buf_idx;
    int32_t sLTP_shp_buf_idx;
    int32_t rand_seed;
    int32_t prev_gain_Q16;
    int32_t rewhite_flag;
} silk_nsq_state;

typedef struct {
    int32_t AnaState[2];                  /* Analysis filterbank state: 0-8 kHz                                   */
    int32_t AnaState1[2];                 /* Analysis filterbank state: 0-4 kHz                                   */
    int32_t AnaState2[2];                 /* Analysis filterbank state: 0-2 kHz                                   */
    int32_t XnrgSubfr[VAD_N_BANDS];       /* Subframe energies                                                    */
    int32_t NrgRatioSmth_Q8[VAD_N_BANDS]; /* Smoothed energy level in each band                                   */
    int16_t HPstate;                      /* State of differentiator in the lowest band                           */
    int32_t NL[VAD_N_BANDS];              /* Noise energy level in each band                                      */
    int32_t inv_NL[VAD_N_BANDS];          /* Inverse noise energy level in each band                              */
    int32_t NoiseLevelBias[VAD_N_BANDS];  /* Noise level estimator bias/offset                                    */
    int32_t counter;                      /* Frame counter used in the initial phase                              */
} silk_VAD_state;

/* Variable cut-off low-pass filter state */
typedef struct {
    int32_t In_LP_State[2];      /* Low pass filter state */
    int32_t transition_frame_no; /* Counter which is mapped to a cut-off frequency */
    int32_t mode;                /* Operating mode, <0: switch down, >0: switch up; 0: do nothing           */
    int32_t saved_fs_kHz;        /* If non-zero, holds the last sampling rate before a bandwidth switching reset. */
} silk_LP_state;

/* Structure containing NLSF codebook */
typedef struct {
    const int16_t  nVectors;
    const int16_t  order;
    const int16_t  quantStepSize_Q16;
    const int16_t  invQuantStepSize_Q6;
    const uint8_t* CB1_NLSF_Q8;
    const int16_t* CB1_Wght_Q9;
    const uint8_t* CB1_iCDF;
    const uint8_t* pred_Q8;
    const uint8_t* ec_sel;
    const uint8_t* ec_iCDF;
    const uint8_t* ec_Rates_Q5;
    const int16_t* deltaMin_Q15;
} silk_NLSF_CB_struct;

typedef struct _silk_resampler_state_struct {
    int32_t sIIR[SILK_RESAMPLER_MAX_IIR_ORDER]; /* this must be the first element of this struct */
    union {
        int32_t i32[SILK_RESAMPLER_MAX_FIR_ORDER];
        int16_t i16[SILK_RESAMPLER_MAX_FIR_ORDER];
    } sFIR;
    int16_t        delayBuf[48];
    int32_t        resampler_function;
    int32_t        batchSize;
    int32_t        invRatio_Q16;
    int32_t        FIR_Order;
    int32_t        FIR_Fracs;
    int32_t        Fs_in_kHz;
    int32_t        Fs_out_kHz;
    int32_t        inputDelay;
    const int16_t* Coefs;
} silk_resampler_state_struct;

typedef struct {
    int16_t pred_prev_Q13[2];
    int16_t sMid[2];
    int16_t sSide[2];
    int32_t mid_side_amp_Q0[4];
    int16_t smth_width_Q14;
    int16_t width_prev_Q14;
    int16_t silent_side_len;
    int8_t  predIx[MAX_FRAMES_PER_PACKET][2][3];
    int8_t  mid_only_flags[MAX_FRAMES_PER_PACKET];
} stereo_enc_state;

typedef struct {
    int16_t pred_prev_Q13[2];
    int16_t sMid[2];
    int16_t sSide[2];
} stereo_dec_state;

/* Struct for Packet Loss Concealment */
typedef struct {
    int32_t pitchL_Q8;              /* Pitch lag to use for voiced concealment                          */
    int16_t LTPCoef_Q14[LTP_ORDER]; /* LTP coeficients to use for voiced concealment                    */
    int16_t prevLPC_Q12[MAX_LPC_ORDER];
    int32_t last_frame_lost; /* Was previous frame lost                                          */
    int32_t rand_seed;       /* Seed for unvoiced signal generation                              */
    int16_t randScale_Q14;   /* Scaling of unvoiced random signal                                */
    int32_t conc_energy;
    int32_t conc_energy_shift;
    int16_t prevLTP_scale_Q14;
    int32_t prevGain_Q16[2];
    int32_t fs_kHz;
    int32_t nb_subfr;
    int32_t subfr_length;
} silk_PLC_struct;

/* Struct for CNG */
typedef struct {
    int32_t CNG_exc_buf_Q14[MAX_FRAME_LENGTH];
    int16_t CNG_smth_NLSF_Q15[MAX_LPC_ORDER];
    int32_t CNG_synth_state[MAX_LPC_ORDER];
    int32_t CNG_smth_Gain_Q16;
    int32_t rand_seed;
    int32_t fs_kHz;
} silk_CNG_struct;

typedef struct {
    int32_t        prev_gain_Q16;
    int32_t        exc_Q14[MAX_FRAME_LENGTH];
    int32_t        sLPC_Q14_buf[MAX_LPC_ORDER];
    int16_t        outBuf[MAX_FRAME_LENGTH + 2 * MAX_SUB_FRAME_LENGTH]; /* Buffer for output signal                    */
    int32_t        lagPrev;                                             /* Previous Lag                                                     */
    int8_t         LastGainIndex;                                       /* Previous gain index                                              */
    int32_t        fs_kHz;                                              /* Sampling frequency in kHz                                        */
    int32_t        fs_API_hz;                                           /* API sample frequency (Hz)                                        */
    int32_t        nb_subfr;                                            /* Number of 5 ms subframes in a frame                              */
    int32_t        frame_length;                                        /* Frame length (samples)                                           */
    int32_t        subfr_length;                                        /* Subframe length (samples)                                        */
    int32_t        ltp_mem_length;                                      /* Length of LTP memory                                             */
    int32_t        LPC_order;                                           /* LPC order                                                        */
    int16_t        prevNLSF_Q15[MAX_LPC_ORDER];                         /* Used to interpolate LSFs                                         */
    int32_t        first_frame_after_reset;                             /* Flag for deactivating NLSF interpolation                         */
    const uint8_t* pitch_lag_low_bits_iCDF;                             /* Pointer to iCDF table for low bits of pitch lag index            */
    const uint8_t* pitch_contour_iCDF;                                  /* Pointer to iCDF table for pitch contour index                    */
    /* For buffering payload in case of more frames per packet */
    int32_t nFramesDecoded;
    int32_t nFramesPerPacket;
    /* Specifically for entropy coding */
    int32_t                     ec_prevSignalType;
    int16_t                     ec_prevLagIndex;
    int32_t                     VAD_flags[MAX_FRAMES_PER_PACKET];
    int32_t                     LBRR_flag;
    int32_t                     LBRR_flags[MAX_FRAMES_PER_PACKET];
    silk_resampler_state_struct resampler_state;
    const silk_NLSF_CB_struct*  psNLSF_CB; /* Pointer to NLSF codebook                                         */
    /* Quantization indices */
    SideInfoIndices indices;
    /* CNG state */
    silk_CNG_struct sCNG;
    /* Stuff used for PLC */
    int32_t         lossCnt;
    int32_t         prevSignalType;
    silk_PLC_struct sPLC;
} silk_decoder_state;

typedef struct {
    int8_t  LastGainIndex;
    int32_t HarmBoost_smth_Q16;
    int32_t HarmShapeGain_smth_Q16;
    int32_t Tilt_smth_Q16;
} silk_shape_state_FIX;

/************************/
/* Decoder control      */
/************************/
typedef struct {
    /* Prediction and coding parameters */
    int32_t pitchL[MAX_NB_SUBFR];
    int32_t Gains_Q16[MAX_NB_SUBFR];
    /* Holds interpolated and final coefficients, 4-byte aligned */
    int16_t PredCoef_Q12[2][MAX_LPC_ORDER];
    int16_t LTPCoef_Q14[LTP_ORDER * MAX_NB_SUBFR];
    int32_t LTP_scale_Q14;
} silk_decoder_control;

/* Decoder Super Struct */
typedef struct {
    stereo_dec_state sStereo;
    int32_t          nChannelsAPI;
    int32_t          nChannelsInternal;
    int32_t          prev_decode_only_middle;
} silk_decoder;

typedef struct {
    int32_t sLPC_Q14[MAX_SUB_FRAME_LENGTH + NSQ_LPC_BUF_LENGTH];
    int32_t RandState[DECISION_DELAY];
    int32_t Q_Q10[DECISION_DELAY];
    int32_t Xq_Q14[DECISION_DELAY];
    int32_t Pred_Q15[DECISION_DELAY];
    int32_t Shape_Q14[DECISION_DELAY];
    int32_t sAR2_Q14[MAX_SHAPE_LPC_ORDER];
    int32_t LF_AR_Q14;
    int32_t Diff_Q14;
    int32_t Seed;
    int32_t SeedInit;
    int32_t RD_Q10;
} NSQ_del_dec_struct;

typedef struct {
    int32_t Q_Q10;
    int32_t RD_Q10;
    int32_t xq_Q14;
    int32_t LF_AR_Q14;
    int32_t Diff_Q14;
    int32_t sLTP_shp_Q14;
    int32_t LPC_exc_Q14;
} NSQ_sample_struct;

typedef NSQ_sample_struct NSQ_sample_pair[2];

/**************************************************************************/
/* Structure for controlling decoder operation and reading decoder status */
/**************************************************************************/
typedef struct {
    /* I:   Number of channels; 1/2                                                         */
    int32_t nChannelsAPI;

    /* I:   Number of channels; 1/2                                                         */
    int32_t nChannelsInternal;

    /* I:   Output signal sampling rate in Hertz; 8000/12000/16000/24000/32000/44100/48000  */
    int32_t API_sampleRate;

    /* I:   Internal sampling rate used, in Hertz; 8000/12000/16000                         */
    int32_t internalSampleRate;

    /* I:   Number of samples per packet in milliseconds; 10/20/40/60                       */
    int32_t payloadSize_ms;

    /* O:   Pitch lag of previous frame (0 if unvoiced), measured in samples at 48 kHz      */
    int32_t prevPitchLag;
} silk_DecControlStruct;

extern const int16_t silk_Quantization_Offsets_Q10[2][2];
extern const uint8_t silk_stereo_pred_joint_iCDF[25];
extern const uint8_t silk_uniform3_iCDF[3];
extern const uint8_t silk_uniform4_iCDF[4];
extern const uint8_t silk_uniform5_iCDF[5];
extern const uint8_t silk_uniform6_iCDF[6];
extern const uint8_t silk_uniform8_iCDF[8];
extern const uint8_t silk_NLSF_EXT_iCDF[7];
extern const uint8_t silk_stereo_only_code_mid_iCDF[2];

// prototypes and inlines

/* silk_min() versions with typecast in the function call */
static inline int32_t silk_min_int(int32_t a, int32_t b) { return (((a) < (b)) ? (a) : (b)); }
static inline int16_t silk_min_16(int16_t a, int16_t b) { return (((a) < (b)) ? (a) : (b)); }
static inline int32_t silk_min_32(int32_t a, int32_t b) { return (((a) < (b)) ? (a) : (b)); }
static inline int64_t silk_min_64(int64_t a, int64_t b) { return (((a) < (b)) ? (a) : (b)); }

/* silk_min() versions with typecast in the function call */
static inline int32_t silk_max_int(int32_t a, int32_t b) { return (((a) > (b)) ? (a) : (b)); }
static inline int16_t silk_max_16(int16_t a, int16_t b) { return (((a) > (b)) ? (a) : (b)); }
static inline int32_t silk_max_32(int32_t a, int32_t b) { return (((a) > (b)) ? (a) : (b)); }
static inline int64_t silk_max_64(int64_t a, int64_t b) { return (((a) > (b)) ? (a) : (b)); }

/* count leading zeros of int32_t64 */
static inline int32_t silk_CLZ64(int64_t in) {
    int32_t in_upper;

    in_upper = (int32_t)silk_RSHIFT64(in, 32);
    if(in_upper == 0) {
        /* Search in the lower 32 bits */
        return 32 + silk_CLZ32((int32_t)in);
    }
    else {
        /* Search in the upper 32 bits */
        return silk_CLZ32(in_upper);
    }
}

/* Rotate a32 right by 'rot' bits. Negative rot values result in rotating left. Output is 32bit int.
   Note: contemporary compilers recognize the C expression below and compile it into a 'ror' instruction if available.
    No need for inline ASM! */
static inline int32_t silk_ROR32(int32_t a32, int32_t rot) {
    uint32_t x = (uint32_t)a32;
    uint32_t r = (uint32_t)rot;
    uint32_t m = (uint32_t)-rot;
    if(rot == 0) { return a32; }
    else if(rot < 0) { return (int32_t)((x << m) | (x >> (32 - m))); }
    else { return (int32_t)((x << (32 - r)) | (x >> r)); }
}

/* get number of leading zeros and fractional part (the bits right after the leading one */
static inline void silk_CLZ_FRAC(int32_t  in,     /* I  input                               */
                                 int32_t* lz,     /* O  number of leading zeros             */
                                 int32_t* frac_Q7 /* O  the 7 bits right after the leading one */
) {
    int32_t lzeros = silk_CLZ32(in);

    *lz = lzeros;
    *frac_Q7 = silk_ROR32(in, 24 - lzeros) & 0x7f;
}

/* Approximation of square root                                          */
/* Accuracy: < +/- 10%  for output values > 15                           */
/*           < +/- 2.5% for output values > 120                          */
static inline int32_t silk_SQRT_APPROX(int32_t x) {
    int32_t y, lz, frac_Q7;

    if(x <= 0) { return 0; }

    silk_CLZ_FRAC(x, &lz, &frac_Q7);

    if(lz & 1) { y = 32768; }
    else { y = 46214; /* 46214 = sqrt(2) * 32768 */ }

    /* get scaling right */
    y >>= silk_RSHIFT(lz, 1);

    /* increment using fractional part of input */
    y = silk_SMLAWB(y, y, silk_SMULBB(213, frac_Q7));

    return y;
}

/* Divide two int32 values and return result as int32 in a given Q-domain */
static inline int32_t silk_DIV32_varQ(                   /* O    returns a good approximation of "(a32 << Qres) / b32" */
                                      const int32_t a32, /* I    numerator (Q0)                  */
                                      const int32_t b32, /* I    denominator (Q0)                */
                                      const int32_t Qres /* I    Q-domain of result (>= 0)       */
) {
    int32_t a_headrm, b_headrm, lshift;
    int32_t b32_inv, a32_nrm, b32_nrm, result;

    assert(b32 != 0);
    assert(Qres >= 0);

    /* Compute number of bits head room and normalize inputs */
    a_headrm = silk_CLZ32(silk_abs(a32)) - 1;
    a32_nrm = silk_LSHIFT(a32, a_headrm); /* Q: a_headrm                  */
    b_headrm = silk_CLZ32(silk_abs(b32)) - 1;
    b32_nrm = silk_LSHIFT(b32, b_headrm); /* Q: b_headrm                  */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = silk_DIV32_16(silk_int32_MAX >> 2, silk_RSHIFT(b32_nrm, 16)); /* Q: 29 + 16 - b_headrm        */

    /* First approximation */
    result = silk_SMULWB(a32_nrm, b32_inv); /* Q: 29 + a_headrm - b_headrm  */

    /* Compute residual by subtracting product of denominator and first approximation */
    /* It's OK to overflow because the final value of a32_nrm should always be small */
    a32_nrm = silk_SUB32_ovflw(a32_nrm, silk_LSHIFT_ovflw(silk_SMMUL(b32_nrm, result), 3)); /* Q: a_headrm   */

    /* Refinement */
    result = silk_SMLAWB(result, a32_nrm, b32_inv); /* Q: 29 + a_headrm - b_headrm  */

    /* Convert to Qres domain */
    lshift = 29 + a_headrm - b_headrm - Qres;
    if(lshift < 0) { return silk_LSHIFT_SAT32(result, -lshift); }
    else {
        if(lshift < 32) { return silk_RSHIFT(result, lshift); }
        else {
            /* Avoid undefined result */
            return 0;
        }
    }
}

/* Invert int32 value and return result as int32 in a given Q-domain */
static inline int32_t silk_INVERSE32_varQ(                   /* O    returns a good approximation of "(1 << Qres) / b32" */
                                          const int32_t b32, /* I    denominator (Q0)                */
                                          const int32_t Qres /* I    Q-domain of result (> 0)        */
) {
    int32_t b_headrm, lshift;
    int32_t b32_inv, b32_nrm, err_Q32, result;

    assert(b32 != 0);
    assert(Qres > 0);

    /* Compute number of bits head room and normalize input */
    b_headrm = silk_CLZ32(silk_abs(b32)) - 1;
    b32_nrm = silk_LSHIFT(b32, b_headrm); /* Q: b_headrm                */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = silk_DIV32_16(silk_int32_MAX >> 2, silk_RSHIFT(b32_nrm, 16)); /* Q: 29 + 16 - b_headrm    */

    /* First approximation */
    result = silk_LSHIFT(b32_inv, 16); /* Q: 61 - b_headrm            */

    /* Compute residual by subtracting product of denominator and first approximation from one */
    err_Q32 = silk_LSHIFT(((int32_t)1 << 29) - silk_SMULWB(b32_nrm, b32_inv), 3); /* Q32                        */

    /* Refinement */
    result = silk_SMLAWW(result, err_Q32, b32_inv); /* Q: 61 - b_headrm            */

    /* Convert to Qres domain */
    lshift = 61 - b_headrm - Qres;
    if(lshift <= 0) { return silk_LSHIFT_SAT32(result, -lshift); }
    else {
        if(lshift < 32) { return silk_RSHIFT(result, lshift); }
        else {
            /* Avoid undefined result */
            return 0;
        }
    }
}

static inline void combine_pulses(int32_t*       out, /* O    combined pulses vector [len] */
                                  const int32_t* in,  /* I    input vector       [2 * len] */
                                  const int32_t  len  /* I    number of OUTPUT samples     */
) {
    int32_t k;
    for(k = 0; k < len; k++) { out[k] = in[2 * k] + in[2 * k + 1]; }
}

void    silk_A2NLSF_trans_poly(int32_t* p, const int32_t dd);
int32_t silk_A2NLSF_eval_poly(int32_t* p, const int32_t x, const int32_t dd);
void    silk_A2NLSF_init(const int32_t* a_Q16, int32_t* P, int32_t* Q, const int32_t dd);
void    silk_A2NLSF(int16_t* NLSF, int32_t* a_Q16, const int32_t d);
void    silk_ana_filt_bank_1(const int16_t* in, int32_t* S, int16_t* outL, int16_t* outH, const int32_t N);
void    silk_biquad_alt_stride1(const int16_t* in, const int32_t* B_Q28, const int32_t* A_Q28, int32_t* S, int16_t* out, const int32_t len);
void    silk_biquad_alt_stride2_c(const int16_t* in, const int32_t* B_Q28, const int32_t* A_Q28, int32_t* S, int16_t* out, const int32_t len);
void    silk_bwexpander_32(int32_t* ar, const int32_t d, int32_t chirp_Q16);
void    silk_bwexpander(int16_t* ar, const int32_t d, int32_t chirp_Q16);
void    silk_stereo_decode_pred(int32_t pred_Q13[]);
void    silk_stereo_decode_mid_only(int32_t* decode_only_mid);
void    silk_PLC_Reset(silk_decoder_state* psDec);
void    silk_PLC(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int16_t frame[], int32_t lost);
void    silk_PLC_glue_frames(silk_decoder_state* psDec, int16_t frame[], int32_t length);
void    silk_LP_interpolate_filter_taps(int32_t B_Q28[TRANSITION_NB], int32_t A_Q28[TRANSITION_NA], const int32_t ind, const int32_t fac_Q16);
void    silk_LP_variable_cutoff(silk_LP_state* psLP, int16_t* frame, const int32_t frame_length);
void    silk_NLSF_unpack(int16_t ec_ix[], uint8_t pred_Q8[], const silk_NLSF_CB_struct* psNLSF_CB, const int32_t CB1_index);
void    silk_NLSF_decode(int16_t* pNLSF_Q15, int8_t* NLSFIndices, const silk_NLSF_CB_struct* psNLSF_CB);
int32_t silk_decoder_set_fs(silk_decoder_state* psDec, int32_t fs_kHz, int32_t fs_API_Hz);
int32_t silk_decode_frame(silk_decoder_state* psDec, int16_t pOut[], int32_t* pN, int32_t lostFlag, int32_t condCoding);
void    silk_decode_indices(silk_decoder_state* psDec, int32_t FrameIndex, int32_t decode_LBRR, int32_t condCoding);
void    silk_decode_parameters(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int32_t condCoding);
void    silk_decode_core(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int16_t xq[], const int16_t pulses[MAX_FRAME_LENGTH]);
void    silk_decode_pulses(int16_t pulses[], const int32_t signalType, const int32_t quantOffsetType, const int32_t frame_length);
int32_t silk_init_decoder(silk_decoder_state* psDec);
int32_t silk_NLSF_del_dec_quant(int8_t indices[], const int16_t x_Q10[], const int16_t w_Q5[], const uint8_t pred_coef_Q8[], const int16_t ec_ix[], const uint8_t ec_rates_Q5[],
                                const int32_t quant_step_size_Q16, const int16_t inv_quant_step_size_Q6, const int32_t mu_Q20, const int16_t order);
void    silk_NLSF_VQ(int32_t err_Q26[], const int16_t in_Q15[], const uint8_t pCB_Q8[], const int16_t pWght_Q9[], const int32_t K, const int32_t LPC_order);
void    silk_LP_variable_cutoff(silk_LP_state* psLP, int16_t* frame, const int32_t frame_length);
int32_t silk_VAD_Init(silk_VAD_state* psSilk_VAD);
void silk_stereo_MS_to_LR(stereo_dec_state* state, int16_t x1[], int16_t x2[], const int32_t pred_Q13[], int32_t fs_kHz, int32_t frame_length);
int32_t  silk_stereo_find_predictor(int32_t* ratio_Q14, const int16_t x[], const int16_t y[], int32_t mid_res_amp_Q0[], int32_t length, int32_t smooth_coef_Q16);
void     silk_stereo_quant_pred(int32_t pred_Q13[], int8_t ix[2][3]);
void     silk_stereo_decode_pred(int32_t pred_Q13[]);
void     silk_stereo_decode_mid_only(int32_t* decode_only_mid);
void     silk_decode_signs(int16_t pulses[], int32_t length, const int32_t signalType, const int32_t quantOffsetType, const int32_t sum_pulses[MAX_NB_SHELL_BLOCKS]);
void     silk_shell_decoder(int16_t* pulses0, const int32_t pulses4);
void     silk_gains_quant(int8_t ind[MAX_NB_SUBFR], int32_t gain_Q16[MAX_NB_SUBFR], int8_t* prev_ind, const int32_t conditional, const int32_t nb_subfr);
void     silk_gains_dequant(int32_t gain_Q16[MAX_NB_SUBFR], const int8_t ind[MAX_NB_SUBFR], int8_t* prev_ind, const int32_t conditional, const int32_t nb_subfr);
int32_t  silk_gains_ID(const int8_t ind[MAX_NB_SUBFR], const int32_t nb_subfr);
void     silk_interpolate(int16_t xi[MAX_LPC_ORDER], const int16_t x0[MAX_LPC_ORDER], const int16_t x1[MAX_LPC_ORDER], const int32_t ifact_Q2, const int32_t d);
void     silk_quant_LTP_gains(int16_t B_Q14[MAX_NB_SUBFR * LTP_ORDER], int8_t cbk_index[MAX_NB_SUBFR], int8_t* periodicity_index, int32_t* sum_gain_dB_Q7, int32_t* pred_gain_dB_Q7,
                              const int32_t XX_Q17[MAX_NB_SUBFR * LTP_ORDER * LTP_ORDER], const int32_t xX_Q17[MAX_NB_SUBFR * LTP_ORDER], const int32_t subfr_len, const int32_t nb_subfr);
void     silk_VQ_WMat_EC_c(int8_t* ind, int32_t* res_nrg_Q15, int32_t* rate_dist_Q8, int32_t* gain_Q7, const int32_t* XX_Q17, const int32_t* xX_Q17, const int8_t* cb_Q7, const uint8_t* cb_gain_Q7,
                           const uint8_t* cl_Q5, const int32_t subfr_len, const int32_t max_gain_Q7, const int32_t L);
void     silk_CNG_Reset(silk_decoder_state* psDec);
void     silk_CNG(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int16_t frame[], int32_t length);
void     silk_CNG_Reset(silk_decoder_state* psDec);
int32_t  silk_Get_Decoder_Size(int32_t* decSizeBytes);

int32_t  silk_InitDecoder();
void     silk_setRawParams(uint8_t channels, uint8_t API_channels, uint8_t payloadSize_ms, uint32_t internalSampleRate, uint32_t API_samleRate);
uint32_t silk_getPrevPitchLag();
int32_t  silk_Decode(int32_t lostFlag, int32_t newPacketFlag, int16_t* samplesOut, int32_t* nSamplesOut);
void     silk_NLSF2A_find_poly(int32_t* out, const int32_t* cLSF, int32_t dd);
void     silk_NLSF2A(int16_t* a_Q12, const int16_t* NLSF, const int32_t d);
void     silk_CNG_exc(int32_t exc_Q14[], int32_t exc_buf_Q14[], int32_t length, int32_t* rand_seed);
void     silk_decode_core(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int16_t xq[], const int16_t pulses[MAX_FRAME_LENGTH]);
int32_t  silk_decode_frame(silk_decoder_state* psDec, int16_t pOut[], int32_t* pN, int32_t lostFlag, int32_t condCoding);
void     silk_decode_pitch(int16_t lagIndex, int8_t contourIndex, int32_t pitch_lags[], const int32_t Fs_kHz, const int32_t nb_subfr);
int32_t  silk_inner_prod_aligned_scale(const int16_t* const inVec1, const int16_t* const inVec2, const int32_t scale, const int32_t len);
int32_t  silk_lin2log(const int32_t inLin);
int32_t  silk_log2lin(const int32_t inLog_Q7);
void     silk_LPC_analysis_filter(int16_t* out, const int16_t* in, const int16_t* B, const int32_t len, const int32_t d);
void     silk_LPC_fit(int16_t* a_QOUT, int32_t* a_QIN, const int32_t QOUT, const int32_t QIN, const int32_t d);
int32_t  LPC_inverse_pred_gain_QA_c(int32_t A_QA[SILK_MAX_ORDER_LPC], const int32_t order);
int32_t  silk_LPC_inverse_pred_gain_c(const int16_t* A_Q12, const int32_t order);
void     silk_NLSF_residual_dequant(int16_t x_Q10[], const int8_t indices[], const uint8_t pred_coef_Q8[], const int32_t quant_step_size_Q16, const int16_t order);
void     silk_NLSF_stabilize(int16_t* NLSF_Q15, const int16_t* NDeltaMin_Q15, const int32_t L);
void     silk_NLSF_VQ_weights_laroia(int16_t* pNLSFW_Q_OUT, const int16_t* pNLSF_Q15, const int32_t D);
void     silk_PLC_update(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl);
void     silk_PLC_energy(int32_t* energy1, int32_t* shift1, int32_t* energy2, int32_t* shift2, const int32_t* exc_Q14, const int32_t* prevGain_Q10, int subfr_length, int nb_subfr);
void     silk_PLC_conceal(silk_decoder_state* psDec, silk_decoder_control* psDecCtrl, int16_t frame[]);
void     silk_PLC_glue_frames(silk_decoder_state* psDec, int16_t frame[], int32_t length);
void     silk_resampler_down2(int32_t* S, int16_t* out, const int16_t* in, int32_t inLen);
void     silk_resampler_private_AR2(int32_t S[], int32_t out_Q8[], const int16_t in[], const int16_t A_Q14[], int32_t len);
int16_t* silk_resampler_private_down_FIR_INTERPOL(int16_t* out, int32_t* buf, const int16_t* FIR_Coefs, int32_t FIR_Order, int32_t FIR_Fracs, int32_t max_index_Q16, int32_t index_increment_Q16);
void     silk_resampler_private_down_FIR(void* SS, int16_t out[], const int16_t in[], int32_t inLen);
int16_t* silk_resampler_private_IIR_FIR_INTERPOL(int16_t* out, int16_t* buf, int32_t max_index_Q16, int32_t index_increment_Q16);
void     silk_resampler_private_IIR_FIR(void* SS, int16_t out[], const int16_t in[], int32_t inLen);
void     silk_resampler_private_up2_HQ(int32_t* S, int16_t* out, const int16_t* in, int32_t len);
void     silk_resampler_private_up2_HQ_wrapper(void* SS, int16_t* out, const int16_t* in, int32_t len);
int32_t  silk_resampler_init(silk_resampler_state_struct* S, int32_t Fs_Hz_in, int32_t Fs_Hz_out, int32_t forEnc);
int32_t  silk_resampler(silk_resampler_state_struct* S, int16_t out[], const int16_t in[], int32_t inLen);
int32_t  silk_sigm_Q15(int32_t in_Q5);
void     silk_insertion_sort_increasing(int32_t* a, int32_t* idx, const int32_t L, const int32_t K);
void     silk_insertion_sort_decreasing_int16(int16_t* a, int32_t* idx, const int32_t L, const int32_t K);
void     silk_insertion_sort_increasing_all_values_int16(int16_t* a, const int32_t L);
void     silk_sum_sqr_shift(int32_t* energy, int32_t* shift, const int16_t* x, int32_t len);






