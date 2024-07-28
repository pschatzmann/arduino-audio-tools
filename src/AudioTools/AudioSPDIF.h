/**
 * @file AudioSPDIF.h
 * @author amedes / Phil Schatzmann
 * @brief S/PDIF output

    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.


 * @date 2022-01-30
 *
 * @copyright  (C) 2020 AMEDES, 2022 Phil Schatzmann
 *
 */

#pragma once

#include "AudioConfig.h"
#include "AudioTools/AudioLogger.h"
#include "AudioTools/AudioStreams.h"
#include "AudioI2S/I2SConfig.h"
#include "AudioI2S/I2SStream.h"

// Default Data Pin
#ifndef SPDIF_DATA_PIN
#define SPDIF_DATA_PIN 23
#endif

#define I2S_NUM ((i2s_port_t)0)
#define I2S_BITS_PER_SAMPLE (32)
#define I2S_CHANNELS 2
#define BMC_BITS_PER_SAMPLE 64
#define BMC_BITS_FACTOR (BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE)
#define SPDIF_BLOCK_SAMPLES 192
#define SPDIF_BUF_DIV 2  // double buffering
#define DMA_BUF_COUNT 2
#define DMA_BUF_LEN                                                  \
  (SPDIF_BLOCK_SAMPLES * BMC_BITS_PER_SAMPLE / I2S_BITS_PER_SAMPLE / \
   SPDIF_BUF_DIV)
#define I2S_BUG_MAGIC (26 * 1000 * 1000)  // magic number for avoiding I2S bug
#define SPDIF_BLOCK_SIZE \
  (SPDIF_BLOCK_SAMPLES * (BMC_BITS_PER_SAMPLE / 8) * I2S_CHANNELS)
#define SPDIF_BUF_SIZE (SPDIF_BLOCK_SIZE / SPDIF_BUF_DIV)
#define SPDIF_BUF_ARRAY_SIZE (SPDIF_BUF_SIZE / sizeof(uint32_t))

// BMC preamble
#define BMC_B 0x33173333  // block start
#define BMC_M 0x331d3333  // left ch
#define BMC_W 0x331b3333  // right ch
#define BMC_MW_DIF (BMC_M ^ BMC_W)
#define SYNC_OFFSET 2  // byte offset of SYNC
#define SYNC_FLIP ((BMC_B ^ BMC_M) >> (SYNC_OFFSET * 8))


namespace audio_tools {

/*
 * 8bit PCM to 16bit BMC conversion table, LSb first, 1 end
 */
static const uint16_t bmc_tab_uint[256] = {
    0x3333, 0xb333, 0xd333, 0x5333, 0xcb33, 0x4b33, 0x2b33, 0xab33, 0xcd33,
    0x4d33, 0x2d33, 0xad33, 0x3533, 0xb533, 0xd533, 0x5533, 0xccb3, 0x4cb3,
    0x2cb3, 0xacb3, 0x34b3, 0xb4b3, 0xd4b3, 0x54b3, 0x32b3, 0xb2b3, 0xd2b3,
    0x52b3, 0xcab3, 0x4ab3, 0x2ab3, 0xaab3, 0xccd3, 0x4cd3, 0x2cd3, 0xacd3,
    0x34d3, 0xb4d3, 0xd4d3, 0x54d3, 0x32d3, 0xb2d3, 0xd2d3, 0x52d3, 0xcad3,
    0x4ad3, 0x2ad3, 0xaad3, 0x3353, 0xb353, 0xd353, 0x5353, 0xcb53, 0x4b53,
    0x2b53, 0xab53, 0xcd53, 0x4d53, 0x2d53, 0xad53, 0x3553, 0xb553, 0xd553,
    0x5553, 0xcccb, 0x4ccb, 0x2ccb, 0xaccb, 0x34cb, 0xb4cb, 0xd4cb, 0x54cb,
    0x32cb, 0xb2cb, 0xd2cb, 0x52cb, 0xcacb, 0x4acb, 0x2acb, 0xaacb, 0x334b,
    0xb34b, 0xd34b, 0x534b, 0xcb4b, 0x4b4b, 0x2b4b, 0xab4b, 0xcd4b, 0x4d4b,
    0x2d4b, 0xad4b, 0x354b, 0xb54b, 0xd54b, 0x554b, 0x332b, 0xb32b, 0xd32b,
    0x532b, 0xcb2b, 0x4b2b, 0x2b2b, 0xab2b, 0xcd2b, 0x4d2b, 0x2d2b, 0xad2b,
    0x352b, 0xb52b, 0xd52b, 0x552b, 0xccab, 0x4cab, 0x2cab, 0xacab, 0x34ab,
    0xb4ab, 0xd4ab, 0x54ab, 0x32ab, 0xb2ab, 0xd2ab, 0x52ab, 0xcaab, 0x4aab,
    0x2aab, 0xaaab, 0xcccd, 0x4ccd, 0x2ccd, 0xaccd, 0x34cd, 0xb4cd, 0xd4cd,
    0x54cd, 0x32cd, 0xb2cd, 0xd2cd, 0x52cd, 0xcacd, 0x4acd, 0x2acd, 0xaacd,
    0x334d, 0xb34d, 0xd34d, 0x534d, 0xcb4d, 0x4b4d, 0x2b4d, 0xab4d, 0xcd4d,
    0x4d4d, 0x2d4d, 0xad4d, 0x354d, 0xb54d, 0xd54d, 0x554d, 0x332d, 0xb32d,
    0xd32d, 0x532d, 0xcb2d, 0x4b2d, 0x2b2d, 0xab2d, 0xcd2d, 0x4d2d, 0x2d2d,
    0xad2d, 0x352d, 0xb52d, 0xd52d, 0x552d, 0xccad, 0x4cad, 0x2cad, 0xacad,
    0x34ad, 0xb4ad, 0xd4ad, 0x54ad, 0x32ad, 0xb2ad, 0xd2ad, 0x52ad, 0xcaad,
    0x4aad, 0x2aad, 0xaaad, 0x3335, 0xb335, 0xd335, 0x5335, 0xcb35, 0x4b35,
    0x2b35, 0xab35, 0xcd35, 0x4d35, 0x2d35, 0xad35, 0x3535, 0xb535, 0xd535,
    0x5535, 0xccb5, 0x4cb5, 0x2cb5, 0xacb5, 0x34b5, 0xb4b5, 0xd4b5, 0x54b5,
    0x32b5, 0xb2b5, 0xd2b5, 0x52b5, 0xcab5, 0x4ab5, 0x2ab5, 0xaab5, 0xccd5,
    0x4cd5, 0x2cd5, 0xacd5, 0x34d5, 0xb4d5, 0xd4d5, 0x54d5, 0x32d5, 0xb2d5,
    0xd2d5, 0x52d5, 0xcad5, 0x4ad5, 0x2ad5, 0xaad5, 0x3355, 0xb355, 0xd355,
    0x5355, 0xcb55, 0x4b55, 0x2b55, 0xab55, 0xcd55, 0x4d55, 0x2d55, 0xad55,
    0x3555, 0xb555, 0xd555, 0x5555,
};
static const int16_t *bmc_tab = (int16_t *)bmc_tab_uint;

static uint32_t spdif_buf[SPDIF_BUF_ARRAY_SIZE];
static uint32_t *spdif_ptr = nullptr;

/**
 * @brief SPDIF configuration
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct SPDIFConfig : public AudioInfo {
  SPDIFConfig() {
    bits_per_sample = 16;
    channels = 2;
    sample_rate = 44100;
  }
  int port_no = 0;  // processor dependent port
  int pin_data = SPDIF_DATA_PIN;
  int buffer_count = 30;
  int buffer_size = 384;
};

/**
 * @brief Output as 16 bit stereo SPDIF on the I2S data output pin
 * @ingroup io
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class SPDIFOutput : public AudioStream {
 public:
  /// default constructor
  SPDIFOutput() = default;

  /// destructor
  virtual ~SPDIFOutput() { end(); }

  /// Starting with last or default settings
  bool begin() { return begin(cfg); }

  /// Start with the provided parameters
  bool begin(SPDIFConfig config) {
    TRACED();
    cfg = config;
    // Some validations to make sure that the config is valid
    if (!(cfg.channels == 1 || cfg.channels == 2)) {
      LOGE("Unsupported number of channels: %d", cfg.channels);
      return false;
    }
    if (cfg.bits_per_sample != 16) {
      LOGE("Unsupported bits per sample: %d - must be 16!",
           cfg.bits_per_sample);
      return false;
    }

    if (i2sOn) {
      i2s.end();
    }

    // initialize S/PDIF buffer
    spdif_buf_init();
    spdif_ptr = spdif_buf;

    // Setup I2S
    int sample_rate = cfg.sample_rate * BMC_BITS_FACTOR;
    if (sample_rate==0){
      TRACEE();
      return false;
    }
    int bclk = sample_rate * I2S_BITS_PER_SAMPLE * I2S_CHANNELS;
    int mclk = (I2S_BUG_MAGIC / bclk) * bclk;  // use mclk for avoiding I2S bug

    I2SConfig i2s_cfg;
    i2s_cfg.sample_rate = sample_rate;
    i2s_cfg.channels = cfg.channels;
#ifndef STM32
    i2s_cfg.pin_ws = -1;
    i2s_cfg.pin_bck = -1;
    i2s_cfg.pin_data = cfg.pin_data;
#endif
    i2s_cfg.buffer_count = cfg.buffer_count;
    i2s_cfg.buffer_size = cfg.buffer_size;
    i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE;
    i2s_cfg.i2s_format = I2S_STD_FORMAT;
    i2s_cfg.rx_tx_mode = TX_MODE;
#ifdef ESP32
    i2s_cfg.use_apll = true;
#  if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0 , 0)
    i2s_cfg.fixed_mclk = mclk;
#  endif
#endif
    i2sOn = i2s.begin(i2s_cfg);
    return i2sOn;
  }

  void end() {
    TRACED();
    i2s.end();
    i2sOn = false;
  }

  /// Change the audio parameters
  void setAudioInfo(AudioInfo info) {
    TRACED();
    if (info.bits_per_sample != 16) {
      LOGE("Unsupported bits per sample: %d - must be 16!",
           info.bits_per_sample);
    }
    if (cfg.bits_per_sample != info.bits_per_sample
    || cfg.channels != info.channels
    || cfg.sample_rate != info.sample_rate
    || !i2sOn) {
      cfg.bits_per_sample = info.bits_per_sample;
      cfg.channels = info.channels;
      cfg.sample_rate = info.sample_rate;
      begin(cfg);
    }
  }

  /// Provides the default configuration
  SPDIFConfig defaultConfig() {
    SPDIFConfig c;
    return c;
  }

  /// Writes the audio data as SPDIF to the defined output pin
  size_t write(const uint8_t *data, size_t len) {
    if (!i2sOn) return 0;
    const uint8_t *p = data;
    size_t result = 0;

    while (p < (uint8_t *)data + len) {
      // convert PCM 16bit data to BMC 32bit pulse pattern
      if (cfg.channels == 2) {
        *(spdif_ptr + 1) =
            (uint32_t)(((bmc_tab[*p] << 16) ^ bmc_tab[*(p + 1)]) << 1) >> 1;
        p += 2;
      } else {
        // must be one channels -> use the same value for both
        *(spdif_ptr + 1) =
            (uint32_t)(((bmc_tab[*p] << 16) ^ bmc_tab[*(p)]) << 1) >> 1;
        p++;
      }
      result += 2;
      spdif_ptr += 2;  // advance to next audio data

      if (spdif_ptr >= &spdif_buf[SPDIF_BUF_ARRAY_SIZE]) {
        // set block start preamble
        ((uint8_t *)spdif_buf)[SYNC_OFFSET] ^= SYNC_FLIP;

        i2s.write((uint8_t *)spdif_buf, sizeof(spdif_buf));
        spdif_ptr = spdif_buf;
      }
    }

    return result;
  }

 protected:
  bool i2sOn = false;
  SPDIFConfig cfg;
  I2SStream i2s;

  // initialize S/PDIF buffer
  void spdif_buf_init(void) {
    TRACED();
    size_t i;
    uint32_t bmc_mw = BMC_W;

    for (i = 0; i < (size_t)SPDIF_BUF_ARRAY_SIZE; i += 2) {
      spdif_buf[i] = bmc_mw ^= BMC_MW_DIF;
    }
  }
};

using SPDIFStream = SPDIFOutput;

}  // namespace audio_tools