/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: structs.h,v 1.49 2009/01/26 23:51:15 menno Exp $
**/
#pragma once
#include "neaacdec.h"

#define MAX_CHANNELS        64
#define MAX_SYNTAX_ELEMENTS 48
#define MAX_WINDOW_GROUPS    8
#define MAX_SFB             51
#define MAX_LTP_SFB         40
#define MAX_LTP_SFB_S        8
#define MAX_ASC_BYTES 64
/* used to save the prediction state */
typedef struct {
    uint16_t   n;
    uint16_t   ifac[15];
    complex_t* work;
    complex_t* tab;
} cfft_info;
typedef struct {
    int16_t r[2];
    int16_t COR[2];
    int16_t VAR[2];
} pred_state;
typedef struct {
    uint16_t   N;
    cfft_info* cfft;
    complex_t* sincos;
    int64_t cycles;
    int64_t fft_cycles;
} mdct_info;
typedef struct {
    const real_t* long_window[2];
    const real_t* short_window[2];
    const real_t* ld_window[2];
    mdct_info* mdct256;
    mdct_info* mdct1024;
    mdct_info* mdct2048;
    int64_t cycles;
} fb_info;
typedef struct {
    uint8_t present;
    uint8_t num_bands;
    uint8_t pce_instance_tag;
    uint8_t excluded_chns_present;
    uint8_t band_top[17];
    uint8_t prog_ref_level;
    uint8_t dyn_rng_sgn[17];
    uint8_t dyn_rng_ctl[17];
    uint8_t exclude_mask[MAX_CHANNELS];
    uint8_t additional_excluded_chns[MAX_CHANNELS];
    real_t  ctrl1;
    real_t  ctrl2;
} drc_info;
typedef struct {
    uint8_t element_instance_tag;
    uint8_t object_type;
    uint8_t sf_index;
    uint8_t num_front_channel_elements;
    uint8_t num_side_channel_elements;
    uint8_t num_back_channel_elements;
    uint8_t num_lfe_channel_elements;
    uint8_t num_assoc_data_elements;
    uint8_t num_valid_cc_elements;
    uint8_t mono_mixdown_present;
    uint8_t mono_mixdown_element_number;
    uint8_t stereo_mixdown_present;
    uint8_t stereo_mixdown_element_number;
    uint8_t matrix_mixdown_idx_present;
    uint8_t pseudo_surround_enable;
    uint8_t matrix_mixdown_idx;
    uint8_t front_element_is_cpe[16];
    uint8_t front_element_tag_select[16];
    uint8_t side_element_is_cpe[16];
    uint8_t side_element_tag_select[16];
    uint8_t back_element_is_cpe[16];
    uint8_t back_element_tag_select[16];
    uint8_t lfe_element_tag_select[16];
    uint8_t assoc_data_element_tag_select[16];
    uint8_t cc_element_is_ind_sw[16];
    uint8_t valid_cc_element_tag_select[16];
    uint8_t channels;
    uint8_t comment_field_bytes;
    uint8_t comment_field_data[257];
    uint8_t num_front_channels; /* extra added values */
    uint8_t num_side_channels;
    uint8_t num_back_channels;
    uint8_t num_lfe_channels;
    uint8_t sce_channel[16];
    uint8_t cpe_channel[16];
} program_config;
typedef struct {
    uint16_t syncword;
    uint8_t  id;
    uint8_t  layer;
    uint8_t  protection_absent;
    uint8_t  profile;
    uint8_t  sf_index;
    uint8_t  private_bit;
    uint8_t  channel_configuration;
    uint8_t  original;
    uint8_t  home;
    uint8_t  emphasis;
    uint8_t  copyright_identification_bit;
    uint8_t  copyright_identification_start;
    uint16_t aac_frame_length;
    uint16_t adts_buffer_fullness;
    uint8_t  no_raw_data_blocks_in_frame;
    uint16_t crc_check;
    uint8_t old_format;/* control param */
} adts_header;
typedef struct {
    uint8_t  copyright_id_present;
    int8_t   copyright_id[10];
    uint8_t  original_copy;
    uint8_t  home;
    uint8_t  bitstream_type;
    uint32_t bitrate;
    uint8_t  num_program_config_elements;
    uint32_t adif_buffer_fullness;
    /* maximum of 16 PCEs */
    program_config pce[16];
} adif_header;
typedef struct {
    uint8_t  last_band;
    uint8_t  data_present;
    uint16_t lag;
    uint8_t  lag_update;
    uint8_t  coef;
    uint8_t  long_used[MAX_SFB];
    uint8_t  short_used[8];
    uint8_t  short_lag_present[8];
    uint8_t  short_lag[8];
} ltp_info;
typedef struct {
    uint8_t limit;
    uint8_t predictor_reset;
    uint8_t predictor_reset_group_number;
    uint8_t prediction_used[MAX_SFB];
} pred_info;
typedef struct {
    uint8_t number_pulse;
    uint8_t pulse_start_sfb;
    uint8_t pulse_offset[4];
    uint8_t pulse_amp[4];
} pulse_info;
typedef struct {
    uint8_t n_filt[8];
    uint8_t coef_res[8];
    uint8_t length[8][4];
    uint8_t order[8][4];
    uint8_t direction[8][4];
    uint8_t coef_compress[8][4];
    uint8_t coef[8][4][32];
} tns_info;
typedef struct {
    uint8_t max_band;
    uint8_t adjust_num[4][8];
    uint8_t alevcode[4][8][8];
    uint8_t aloccode[4][8][8];
} ssr_info;
typedef struct {
    uint8_t    max_sfb;
    uint8_t    num_swb;
    uint8_t    num_window_groups;
    uint8_t    num_windows;
    uint8_t    window_sequence;
    uint8_t    window_group_length[8];
    uint8_t    window_shape;
    uint8_t    scale_factor_grouping;
    uint16_t   sect_sfb_offset[8][15 * 8];
    uint16_t   swb_offset[52];
    uint16_t   swb_offset_max;
    uint8_t    sect_cb[8][15 * 8];
    uint16_t   sect_start[8][15 * 8];
    uint16_t   sect_end[8][15 * 8];
    uint8_t    sfb_cb[8][8 * 15];
    uint8_t    num_sec[8]; /* number of sections in a group */
    uint8_t    global_gain;
    int16_t    scale_factors[8][51]; /* [0..255] */
    uint8_t    ms_mask_present;
    uint8_t    ms_used[MAX_WINDOW_GROUPS][MAX_SFB];
    uint8_t    noise_used;
    uint8_t    is_used;
    uint8_t    pulse_data_present;
    uint8_t    tns_data_present;
    uint8_t    gain_control_data_present;
    uint8_t    predictor_data_present;
    pulse_info pul;
    tns_info   tns;
    pred_info pred;
    ltp_info ltp;
    ltp_info ltp2;
    ssr_info ssr;
    uint16_t length_of_reordered_spectral_data; /* ER HCR data */
    uint8_t  length_of_longest_codeword;
    uint8_t  sf_concealment;/* ER RLVC data */
    uint8_t  rev_global_gain;
    uint16_t length_of_rvlc_sf;
    uint16_t dpcm_noise_nrg;
    uint8_t  sf_escapes_present;
    uint8_t  length_of_rvlc_escapes;
    uint16_t dpcm_noise_last_position;
} ic_stream; /* individual channel stream */
typedef struct {
    uint8_t   channel;
    int16_t   paired_channel;
    uint8_t   element_instance_tag;
    uint8_t   common_window;
    ic_stream ics1;
    ic_stream ics2;
} element; /* syntax element (SCE, CPE, LFE) */
typedef struct {
    int      inited;
    int      version, versionA;
    int      framelen_type;
    int      useSameStreamMux;
    int      allStreamsSameTimeFraming;
    int      numSubFrames;
    int      numPrograms;
    int      numLayers;
    int      otherDataPresent;
    uint32_t otherDataLenBits;
    uint32_t frameLength;
    uint8_t  ASC[MAX_ASC_BYTES];
    uint32_t ASCbits;
} latm_header;
typedef struct NeAACDecConfiguration
{
    unsigned char defObjectType;
    unsigned long defSampleRate;
    unsigned char outputFormat;
    unsigned char downMatrix;
    unsigned char useOldADTSFormat;
    unsigned char dontUpSampleImplicitSBR;
} NeAACDecConfiguration, *NeAACDecConfigurationPtr;
typedef struct
{
    uint8_t drm_ps_data_available;
    uint8_t bs_enable_sa;
    uint8_t bs_enable_pan;
    uint8_t bs_sa_dt_flag;
    uint8_t bs_pan_dt_flag;
    uint8_t g_last_had_sa;
    uint8_t g_last_had_pan;
    int8_t bs_sa_data[DRM_NUM_SA_BANDS];
    int8_t bs_pan_data[DRM_NUM_PAN_BANDS];
    int8_t g_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_pan_index[DRM_NUM_PAN_BANDS];
    int8_t g_prev_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_prev_pan_index[DRM_NUM_PAN_BANDS];
    int8_t sa_decode_error;
    int8_t pan_decode_error;
    int8_t g_last_good_sa_index[DRM_NUM_SA_BANDS];
    int8_t g_last_good_pan_index[DRM_NUM_PAN_BANDS];
    qmf_t SA[NUM_OF_SUBSAMPLES][MAX_SA_BAND];
    complex_t d_buff[2][MAX_SA_BAND];
    complex_t d2_buff[NUM_OF_LINKS][MAX_DELAY][MAX_SA_BAND];
    uint8_t delay_buf_index_ser[NUM_OF_LINKS];
    real_t prev_nrg[MAX_SA_BAND];
    real_t prev_peakdiff[MAX_SA_BAND];
    real_t peakdecay_fast[MAX_SA_BAND];
} drm_ps_info;
typedef struct
{
    /* bitstream parameters */
    uint8_t enable_iid;
    uint8_t enable_icc;
    uint8_t enable_ext;
    uint8_t iid_mode;
    uint8_t icc_mode;
    uint8_t nr_iid_par;
    uint8_t nr_ipdopd_par;
    uint8_t nr_icc_par;
    uint8_t frame_class;
    uint8_t num_env;
    uint8_t border_position[MAX_PS_ENVELOPES+1];
    uint8_t iid_dt[MAX_PS_ENVELOPES];
    uint8_t icc_dt[MAX_PS_ENVELOPES];
    uint8_t enable_ipdopd;
    uint8_t ipd_mode;
    uint8_t ipd_dt[MAX_PS_ENVELOPES];
    uint8_t opd_dt[MAX_PS_ENVELOPES];
    /* indices */
    int8_t iid_index_prev[34];
    int8_t icc_index_prev[34];
    int8_t ipd_index_prev[17];
    int8_t opd_index_prev[17];
    int8_t iid_index[MAX_PS_ENVELOPES][34];
    int8_t icc_index[MAX_PS_ENVELOPES][34];
    int8_t ipd_index[MAX_PS_ENVELOPES][17];
    int8_t opd_index[MAX_PS_ENVELOPES][17];
    int8_t ipd_index_1[17];
    int8_t opd_index_1[17];
    int8_t ipd_index_2[17];
    int8_t opd_index_2[17];
    /* ps data was correctly read */
    uint8_t ps_data_available;
    /* a header has been read */
    uint8_t header_read;
    /* hybrid filterbank parameters */
    void *hyb;
    uint8_t use34hybrid_bands;
    uint8_t numTimeSlotsRate;
    /**/
    uint8_t num_groups;
    uint8_t num_hybrid_groups;
    uint8_t nr_par_bands;
    uint8_t nr_allpass_bands;
    uint8_t decay_cutoff;
    uint8_t *group_border;
    uint16_t *map_group2bk;
    /* filter delay handling */
    uint8_t saved_delay;
    uint8_t delay_buf_index_ser[NO_ALLPASS_LINKS];
    uint8_t num_sample_delay_ser[NO_ALLPASS_LINKS];
    uint8_t delay_D[64];
    uint8_t delay_buf_index_delay[64];
    complex_t delay_Qmf[14][64]; /* 14 samples delay max, 64 QMF channels */
    complex_t delay_SubQmf[2][32]; /* 2 samples delay max (SubQmf is always allpass filtered) */
    complex_t delay_Qmf_ser[NO_ALLPASS_LINKS][5][64]; /* 5 samples delay max (table 8.34), 64 QMF channels */
    complex_t delay_SubQmf_ser[NO_ALLPASS_LINKS][5][32]; /* 5 samples delay max (table 8.34) */
    /* transients */
    real_t alpha_decay;
    real_t alpha_smooth;
    real_t P_PeakDecayNrg[34];
    real_t P_prev[34];
    real_t P_SmoothPeakDecayDiffNrg_prev[34];
    /* mixing and phase */
    complex_t h11_prev[50];
    complex_t h12_prev[50];
    complex_t h21_prev[50];
    complex_t h22_prev[50];
    uint8_t phase_hist;
    complex_t ipd_prev[20][2];
    complex_t opd_prev[20][2];
} ps_info;
typedef struct {
    real_t *x;
    int16_t x_index;
    uint8_t channels;
} qmfa_info;
typedef struct {
    real_t *v;
    int16_t v_index;
    uint8_t channels;
} qmfs_info;
typedef struct{
    uint32_t sample_rate;
    uint32_t maxAACLine;
    uint8_t rate;
    uint8_t just_seeked;
    uint8_t ret;
    uint8_t amp_res[2];
    uint8_t k0;
    uint8_t kx;
    uint8_t M;
    uint8_t N_master;
    uint8_t N_high;
    uint8_t N_low;
    uint8_t N_Q;
    uint8_t N_L[4];
    uint8_t n[2];
    uint8_t f_master[64];
    uint8_t f_table_res[2][64];
    uint8_t f_table_noise[64];
    uint8_t f_table_lim[4][64];
    uint8_t f_group[5][64];
    uint8_t N_G[5];
    uint8_t table_map_k_to_g[64];
    uint8_t abs_bord_lead[2];
    uint8_t abs_bord_trail[2];
    uint8_t n_rel_lead[2];
    uint8_t n_rel_trail[2];
    uint8_t L_E[2];
    uint8_t L_E_prev[2];
    uint8_t L_Q[2];
    uint8_t t_E[2][MAX_L_E+1];
    uint8_t t_Q[2][3];
    uint8_t f[2][MAX_L_E+1];
    uint8_t f_prev[2];
    real_t *G_temp_prev[2][5];
    real_t *Q_temp_prev[2][5];
    int8_t GQ_ringbuf_index[2];
    int16_t E[2][64][MAX_L_E];
    int16_t E_prev[2][64];
    real_t E_orig[2][64][MAX_L_E];
    real_t E_curr[2][64][MAX_L_E];
    int32_t Q[2][64][2];
    real_t Q_div[2][64][2];
    real_t Q_div2[2][64][2];
    int32_t Q_prev[2][64];
    int8_t l_A[2];
    int8_t l_A_prev[2];
    uint8_t bs_invf_mode[2][MAX_L_E];
    uint8_t bs_invf_mode_prev[2][MAX_L_E];
    real_t bwArray[2][64];
    real_t bwArray_prev[2][64];
    uint8_t noPatches;
    uint8_t patchNoSubbands[64];
    uint8_t patchStartSubband[64];
    uint8_t bs_add_harmonic[2][64];
    uint8_t bs_add_harmonic_prev[2][64];
    uint16_t index_noise_prev[2];
    uint8_t psi_is_prev[2];
    uint8_t bs_start_freq_prev;
    uint8_t bs_stop_freq_prev;
    uint8_t bs_xover_band_prev;
    uint8_t bs_freq_scale_prev;
    uint8_t bs_alter_scale_prev;
    uint8_t bs_noise_bands_prev;
    int8_t prevEnvIsShort[2];
    int8_t kx_prev;
    uint8_t bsco;
    uint8_t bsco_prev;
    uint8_t M_prev;
    uint16_t frame_len;
    uint8_t Reset;
    uint32_t frame;
    uint32_t header_count;
    uint8_t id_aac;
    qmfa_info *qmfa[2];
    qmfs_info *qmfs[2];
    qmf_t Xsbr[2][MAX_NTSRHFG][64];
    uint8_t Is_DRM_SBR;
    drm_ps_info *drm_ps;
    uint8_t numTimeSlotsRate;
    uint8_t numTimeSlots;
    uint8_t tHFGen;
    uint8_t tHFAdj;
    ps_info *ps;
    uint8_t ps_used;
    uint8_t psResetFlag;
    /* to get it compiling */
    /* we'll see during the coding of all the tools, whether
       these are all used or not.
    */
    uint8_t bs_header_flag;
    uint8_t bs_crc_flag;
    uint16_t bs_sbr_crc_bits;
    uint8_t bs_protocol_version;
    uint8_t bs_amp_res;
    uint8_t bs_start_freq;
    uint8_t bs_stop_freq;
    uint8_t bs_xover_band;
    uint8_t bs_freq_scale;
    uint8_t bs_alter_scale;
    uint8_t bs_noise_bands;
    uint8_t bs_limiter_bands;
    uint8_t bs_limiter_gains;
    uint8_t bs_interpol_freq;
    uint8_t bs_smoothing_mode;
    uint8_t bs_samplerate_mode;
    uint8_t bs_add_harmonic_flag[2];
    uint8_t bs_add_harmonic_flag_prev[2];
    uint8_t bs_extended_data;
    uint8_t bs_extension_id;
    uint8_t bs_extension_data;
    uint8_t bs_coupling;
    uint8_t bs_frame_class[2];
    uint8_t bs_rel_bord[2][9];
    uint8_t bs_rel_bord_0[2][9];
    uint8_t bs_rel_bord_1[2][9];
    uint8_t bs_pointer[2];
    uint8_t bs_abs_bord_0[2];
    uint8_t bs_abs_bord_1[2];
    uint8_t bs_num_rel_0[2];
    uint8_t bs_num_rel_1[2];
    uint8_t bs_df_env[2][9];
    uint8_t bs_df_noise[2][3];
} sbr_info;
typedef struct {
    uint8_t adts_header_present;
    uint8_t adif_header_present;
    uint8_t latm_header_present;
    uint8_t sf_index;
    uint8_t object_type;
    uint8_t channelConfiguration;
    uint8_t aacSectionDataResilienceFlag;
    uint8_t aacScalefactorDataResilienceFlag;
    uint8_t aacSpectralDataResilienceFlag;
    uint16_t frameLength;
    uint8_t  postSeekResetFlag;
    uint32_t frame;
    uint8_t  downMatrix;
    uint8_t  upMatrix;
    uint8_t  first_syn_ele;
    uint8_t  has_lfe;
    uint8_t fr_channels;  /* number of channels in current frame */
    uint8_t fr_ch_ele; /* number of elements in current frame */
    uint8_t element_output_channels[MAX_SYNTAX_ELEMENTS]; /* element_output_channels:  determines the number of channels the element will output */
    uint8_t element_alloced[MAX_SYNTAX_ELEMENTS];/* element_alloced:determines whether the data needed for the element is allocated or not*/
    uint8_t alloced_channels;  /* alloced_channels: determines the number of channels where output data is allocated for*/
    void*   sample_buffer; /* output data buffer */
    uint8_t window_shape_prev[MAX_CHANNELS];
    uint16_t ltp_lag[MAX_CHANNELS];
    fb_info*  fb;
    drc_info* drc;
    real_t*   time_out[MAX_CHANNELS];
    real_t*   fb_intermed[MAX_CHANNELS];
    int8_t sbr_present_flag;
    int8_t forceUpSampling;
    int8_t downSampledSBR;
    uint8_t   sbr_alloced[MAX_SYNTAX_ELEMENTS];  /* determines whether SBR data is allocated for the gives element */
    sbr_info* sbr[MAX_SYNTAX_ELEMENTS];
    uint8_t ps_used[MAX_SYNTAX_ELEMENTS];
    uint8_t ps_used_global;
    real_t* ssr_overlap[MAX_CHANNELS];
    real_t* prev_fmd[MAX_CHANNELS];
    real_t  ipqf_buffer[MAX_CHANNELS][4][96 / 4];
    pred_state* pred_stat[MAX_CHANNELS];
    int16_t* lt_pred_stat[MAX_CHANNELS];
    uint8_t error_state;
    uint32_t __r1; /* RNG states */
    uint32_t __r2;
    uint8_t        pce_set;/* Program Config Element */
    program_config pce;
    uint8_t        element_id[MAX_CHANNELS];
    uint8_t        internal_channel[MAX_CHANNELS];
    NeAACDecConfiguration config; /* Configuration data */
    int64_t cycles;
    int64_t spectral_cycles;
    int64_t output_cycles;
    int64_t scalefac_cycles;
    int64_t requant_cycles;
    latm_header          latm_config;
    const unsigned char* cmes;
    uint8_t              isPS;
} NeAACDecStruct;
/* 1st step table */
typedef struct {
    uint8_t offset;
    uint8_t extra_bits;
} hcb;
/* 2nd step table with quadruple data */
typedef struct {
    uint8_t bits;
    int8_t  x;
    int8_t  y;
} hcb_2_pair;
typedef struct {
    uint8_t bits;
    int8_t  x;
    int8_t  y;
    int8_t  v;
    int8_t  w;
} hcb_2_quad;
/* binary search table */
typedef struct {
    uint8_t is_leaf;
    int8_t  data[4];
} hcb_bin_quad;
typedef struct {
    uint8_t is_leaf;
    int8_t  data[2];
} hcb_bin_pair;
typedef struct _bitfile {
    /* bit input */
    uint32_t    bufa;
    uint32_t    bufb;
    uint32_t    bits_left;
    uint32_t    buffer_size; /* size of the buffer in bytes */
    uint32_t    bytes_left;
    uint8_t     error;
    uint32_t*   tail;
    uint32_t*   start;
    const void* buffer;
} bitfile;
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef ERROR_RESILIENCE
/* Modified bit reading functions for HCR */
#endif /*ERROR_RESILIENCE*/
typedef struct {
    /* bit input */
    uint32_t bufa;
    uint32_t bufb;
    int8_t   len;
} bits_t;
typedef struct {
    uint8_t  cb;
    uint8_t  decoded;
    uint16_t sp_offset;
    bits_t   bits;
} codeword_t;
typedef struct
{
    int8_t index;
    uint8_t len;
    uint32_t cw;
} rvlc_huff_table;
// ————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef PS_DEC
/* type definitions */
typedef struct {
    uint8_t frame_len;
    uint8_t resolution20[3];
    uint8_t resolution34[5];
    qmf_t*  work;
    qmf_t** buffer;
    qmf_t** temp;
} hyb_info;
#endif //  PS_DEC
typedef struct {
    real_t G_lim_boost[MAX_L_E][MAX_M];
    real_t Q_M_lim_boost[MAX_L_E][MAX_M];
    real_t S_M_boost[MAX_L_E][MAX_M];
} sbr_hfadj_info;
// ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
#ifdef SBR_DEC
typedef struct {
    complex_t r01;
    complex_t r02;
    complex_t r11;
    complex_t r12;
    complex_t r22;
    real_t    det;
} acorr_coef;
#endif // SBR_DEC