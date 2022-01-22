#pragma once

#include "AudioConfig.h"
#include "AudioI2S/I2SConfig.h"
#include "AudioI2S/I2SStream.h"
#include "AudioTools/AudioStreams.h"

#define LEFTCHANNEL 0
#define RIGHTCHANNEL 1

namespace audio_tools {

typedef int16_t frame[2];

// BMC (Biphase Mark Coded) values (bit order reversed, i.e. LSB first)
static const uint16_t spdif_bmclookup[256] PROGMEM = {
    0xcccc, 0x4ccc, 0x2ccc, 0xaccc, 0x34cc, 0xb4cc, 0xd4cc, 0x54cc, 0x32cc,
    0xb2cc, 0xd2cc, 0x52cc, 0xcacc, 0x4acc, 0x2acc, 0xaacc, 0x334c, 0xb34c,
    0xd34c, 0x534c, 0xcb4c, 0x4b4c, 0x2b4c, 0xab4c, 0xcd4c, 0x4d4c, 0x2d4c,
    0xad4c, 0x354c, 0xb54c, 0xd54c, 0x554c, 0x332c, 0xb32c, 0xd32c, 0x532c,
    0xcb2c, 0x4b2c, 0x2b2c, 0xab2c, 0xcd2c, 0x4d2c, 0x2d2c, 0xad2c, 0x352c,
    0xb52c, 0xd52c, 0x552c, 0xccac, 0x4cac, 0x2cac, 0xacac, 0x34ac, 0xb4ac,
    0xd4ac, 0x54ac, 0x32ac, 0xb2ac, 0xd2ac, 0x52ac, 0xcaac, 0x4aac, 0x2aac,
    0xaaac, 0x3334, 0xb334, 0xd334, 0x5334, 0xcb34, 0x4b34, 0x2b34, 0xab34,
    0xcd34, 0x4d34, 0x2d34, 0xad34, 0x3534, 0xb534, 0xd534, 0x5534, 0xccb4,
    0x4cb4, 0x2cb4, 0xacb4, 0x34b4, 0xb4b4, 0xd4b4, 0x54b4, 0x32b4, 0xb2b4,
    0xd2b4, 0x52b4, 0xcab4, 0x4ab4, 0x2ab4, 0xaab4, 0xccd4, 0x4cd4, 0x2cd4,
    0xacd4, 0x34d4, 0xb4d4, 0xd4d4, 0x54d4, 0x32d4, 0xb2d4, 0xd2d4, 0x52d4,
    0xcad4, 0x4ad4, 0x2ad4, 0xaad4, 0x3354, 0xb354, 0xd354, 0x5354, 0xcb54,
    0x4b54, 0x2b54, 0xab54, 0xcd54, 0x4d54, 0x2d54, 0xad54, 0x3554, 0xb554,
    0xd554, 0x5554, 0x3332, 0xb332, 0xd332, 0x5332, 0xcb32, 0x4b32, 0x2b32,
    0xab32, 0xcd32, 0x4d32, 0x2d32, 0xad32, 0x3532, 0xb532, 0xd532, 0x5532,
    0xccb2, 0x4cb2, 0x2cb2, 0xacb2, 0x34b2, 0xb4b2, 0xd4b2, 0x54b2, 0x32b2,
    0xb2b2, 0xd2b2, 0x52b2, 0xcab2, 0x4ab2, 0x2ab2, 0xaab2, 0xccd2, 0x4cd2,
    0x2cd2, 0xacd2, 0x34d2, 0xb4d2, 0xd4d2, 0x54d2, 0x32d2, 0xb2d2, 0xd2d2,
    0x52d2, 0xcad2, 0x4ad2, 0x2ad2, 0xaad2, 0x3352, 0xb352, 0xd352, 0x5352,
    0xcb52, 0x4b52, 0x2b52, 0xab52, 0xcd52, 0x4d52, 0x2d52, 0xad52, 0x3552,
    0xb552, 0xd552, 0x5552, 0xccca, 0x4cca, 0x2cca, 0xacca, 0x34ca, 0xb4ca,
    0xd4ca, 0x54ca, 0x32ca, 0xb2ca, 0xd2ca, 0x52ca, 0xcaca, 0x4aca, 0x2aca,
    0xaaca, 0x334a, 0xb34a, 0xd34a, 0x534a, 0xcb4a, 0x4b4a, 0x2b4a, 0xab4a,
    0xcd4a, 0x4d4a, 0x2d4a, 0xad4a, 0x354a, 0xb54a, 0xd54a, 0x554a, 0x332a,
    0xb32a, 0xd32a, 0x532a, 0xcb2a, 0x4b2a, 0x2b2a, 0xab2a, 0xcd2a, 0x4d2a,
    0x2d2a, 0xad2a, 0x352a, 0xb52a, 0xd52a, 0x552a, 0xccaa, 0x4caa, 0x2caa,
    0xacaa, 0x34aa, 0xb4aa, 0xd4aa, 0x54aa, 0x32aa, 0xb2aa, 0xd2aa, 0x52aa,
    0xcaaa, 0x4aaa, 0x2aaa, 0xaaaa};

const uint32_t VUCP_PREAMBLE_B = 0xCCE80000;  // 11001100 11101000
const uint32_t VUCP_PREAMBLE_M = 0xCCE20000;  // 11001100 11100010
const uint32_t VUCP_PREAMBLE_W = 0xCCE40000;  // 11001100 11100100

/**
 * @brief SPDIF configuration
 * We only support 16 bits per sample
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
struct SPDIFConfig {
  int port_no = 0;  // processor dependent port
  int pin_data = PIN_I2S_DATA_OUT;
  int sample_rate = 44100;
  int channels = 2;
};

/**
 * @brief Output as SPDIF on the data output pin
 * @author Phil Schatzmann
 * @copyright GPLv3
 *
 */
class SPDIFStream : public AudioStreamX {
 public:
  SPDIFStream() = default;

  /// Starting with default settings
  bool begin() { return begin(defaultConfig()); }

  /// Start with the provided parameters
  bool begin(SPDIFConfig cfg) {
    this->cfg = cfg;
    I2SConfig i2s_cfg;
    i2s_cfg.sample_rate = cfg.sample_rate * 2;  // 2 x sampling_rate
    i2s_cfg.bits_per_sample = 32;
    i2s_cfg.use_apll = true;
    i2s_cfg.channels = 2;
    i2s_cfg.pin_ws = -1;
    i2s_cfg.pin_bck = -1;
    i2s_cfg.pin_data = cfg.pin_data;

    i2s.begin(i2s_cfg);
    i2sOn = true;
    frame_num = 0;
    return true;
  }

  /// Change the audio parameters
  virtual void setAudioInfo(AudioBaseInfo info) {
    end();
    cfg.channels = info.channels;
    cfg.sample_rate =  info.sample_rate;
    if (info.bits_per_sample != 16) {
      LOGE("Unsupported bits per sample: %d - must be 16!",
           info.bits_per_sample);
    }
    begin(cfg);
  }

  /// Provides the default configuration
  SPDIFConfig defaultConfig() {
    SPDIFConfig c;
    return c;
  }

  bool end() {
    i2s.end();
    frame_num = 0;
    i2sOn = false;
    return true;
  }

  /// Writes the audio data as SPDIF to the defined output pin
  size_t write(const uint8_t *values, size_t len) {
    size_t result = 0;
    int16_t *v = (int16_t *)values;
    if (cfg.channels == 2) {
      for (int j = 0; j < len / 32; j++) {
        // provide the 2 channels from the data
        result += processFrame(v[j], v[j + 1]);
      }
    } else if (cfg.channels == 1) {
      for (int j = 0; j < len / 16; j++) {
        // generate 2 channels
        result += processFrame(v[j], v[j]);
      }
    }
    return result;
  }

 protected:
  SPDIFConfig cfg;
  I2SStream i2s;
  bool i2sOn;
  uint8_t frame_num = 0;

  // process a single frame
  int processFrame(int16_t left, int16_t right) {
    if (!i2sOn) return true;  // Sink the data
    int16_t ms[2];
    uint16_t hi, lo, aux;
    uint32_t buf[4];

    ms[0] = left;
    ms[1] = right;

    // S/PDIF encoding:
    //   http://www.hardwarebook.info/S/PDIF
    // Original sources: Teensy Audio Library
    //   https://github.com/PaulStoffregen/Audio/blob/master/output_spdif2.cpp
    //
    // Order of bits, before BMC encoding, from the definition of SPDIF format
    //   PPPP AAAA  SSSS SSSS  SSSS SSSS  SSSS VUCP
    // are sent rearanged as
    //   VUCP PPPP  AAAA 0000  SSSS SSSS  SSSS SSSS
    // This requires a bit less shifting as 16 sample bits align and can be
    // BMC encoded with two table lookups (and at the same time flipped to LSB
    // first). There is no separate word-clock, so hopefully the receiver won't
    // notice.

    uint16_t sample_left = ms[LEFTCHANNEL];
    // BMC encode and flip left channel bits
    hi = spdif_bmclookup[(uint8_t)(sample_left >> 8)];
    lo = spdif_bmclookup[(uint8_t)sample_left];
    // Low word is inverted depending on first bit of high word
    lo ^= (~((int16_t)hi) >> 16);
    buf[0] = ((uint32_t)lo << 16) | hi;
    // Fixed 4 bits auxillary-audio-databits, the first used as parity
    // Depending on first bit of low word, invert the bits
    aux = 0xb333 ^ (((uint32_t)((int16_t)lo)) >> 17);
    // Send 'B' preamble only for the first frame of data-block
    if (frame_num == 0) {
      buf[1] = VUCP_PREAMBLE_B | aux;
    } else {
      buf[1] = VUCP_PREAMBLE_M | aux;
    }

    uint16_t sample_right = ms[RIGHTCHANNEL];
    // BMC encode right channel, similar as above
    hi = spdif_bmclookup[(uint8_t)(sample_right >> 8)];
    lo = spdif_bmclookup[(uint8_t)sample_right];
    lo ^= (~((int16_t)hi) >> 16);
    buf[2] = ((uint32_t)lo << 16) | hi;
    aux = 0xb333 ^ (((uint32_t)((int16_t)lo)) >> 17);
    buf[3] = VUCP_PREAMBLE_W | aux;

    // Assume DMA buffers are multiples of 16 bytes. Either we write all bytes
    // or none.
    int len = 8 * cfg.channels;
    size_t bytes_written = i2s.write((const uint8_t *)&buf, len);
    // If we didn't write all bytes, return false early and do not increment
    // frame_num
    if (bytes_written != len) return 0;
    // Increment and rotate frame number
    if (++frame_num > 191) frame_num = 0;
    return len;
  }
};

}