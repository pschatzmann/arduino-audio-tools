#pragma once

#include "AudioTools/CoreAudio/AudioI2S/I2SConfig.h"
#if defined(RP2040_HOWER)
#include <I2S.h>

#define IS_I2S_IMPLEMENTED 

namespace audio_tools {

/**
 * @brief Basic I2S API - for the ...
 * @ingroup platform
 * @author Phil Schatzmann
 * @author LinusHeu
 * @copyright GPLv3
 */
class I2SDriverRP2040 {
  friend class I2SStream;

 public:
  /// Provides the default configuration
  I2SConfigStd defaultConfig(RxTxMode mode) {
    I2SConfigStd c(mode);
    return c;
  }
  /// Potentially updates values dynamically
  bool setAudioInfo(AudioInfo info) { 
    if (info.sample_rate != cfg.sample_rate && !i2s.setFrequency(info.sample_rate)) {
      LOGI("i2s.setFrequency %d failed", info.sample_rate);
      return false;
    }
    if (info.bits_per_sample != cfg.bits_per_sample && !i2s.setBitsPerSample(info.bits_per_sample)) {
      LOGI("i2s.setBitsPerSample %d failed", info.bits_per_sample);
      return false;
    }
    cfg.sample_rate = info.sample_rate;
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.channels = info.channels;
    return true; 
  }

  /// starts the DAC with the default config in TX Mode
  bool begin(RxTxMode mode = TX_MODE) {
    TRACED();
    return begin(defaultConfig(mode));
  }

  /// starts the DAC
  bool begin(I2SConfigStd cfg) {
    TRACEI();
    // prevent multiple begins w/o calling end
    if (is_active) end();
    this->cfg = cfg;
    cfg.logInfo();
    switch (cfg.rx_tx_mode) {
      case TX_MODE:
        i2s = I2S(OUTPUT);
        break;
      case RX_MODE:
        i2s = I2S(INPUT);
        has_input[0] = has_input[1] = false;
        break;
      default:
        LOGE("Unsupported mode: only TX_MODE is supported");
        return false;
        break;
    }

    if (cfg.pin_ws == cfg.pin_bck + 1) {  // normal pin order
      if (!i2s.setBCLK(cfg.pin_bck)) {
        LOGE("Could not set bck pin: %d", cfg.pin_bck);
        return false;
      }
    } else if (cfg.pin_ws == cfg.pin_bck - 1) {  // reverse pin order
      if (!i2s.swapClocks() ||
          !i2s.setBCLK(
              cfg.pin_ws)) {  // setBCLK() actually sets the lower pin of bck/ws
        LOGE("Could not set bck pin: %d", cfg.pin_bck);
        return false;
      }
    } else {
      LOGE("pins bck: '%d' and ws: '%d' must be next to each other",
           cfg.pin_bck, cfg.pin_ws);
      return false;
    }
    if (!i2s.setDATA(cfg.pin_data)) {
      LOGE("Could not set data pin: %d", cfg.pin_data);
      return false;
    }
    if (cfg.pin_mck != -1) {
      i2s.setMCLKmult(cfg.mck_multiplier);
      if (!i2s.setMCLK(cfg.pin_mck)) {
        LOGE("Could not set data pin: %d", cfg.pin_mck);
        return false;
      }
    }

    if (cfg.bits_per_sample == 8 ||
        !i2s.setBitsPerSample(cfg.bits_per_sample)) {
      LOGE("Could not set bits per sample: %d", cfg.bits_per_sample);
      return false;
    }

    if (!i2s.setBuffers(cfg.buffer_count, cfg.buffer_size)) {
      LOGE("Could not set buffers: Count: '%d', size: '%d'", cfg.buffer_count,
           cfg.buffer_size);
      return false;
    }

    // setup format
    if (cfg.i2s_format == I2S_STD_FORMAT ||
        cfg.i2s_format == I2S_PHILIPS_FORMAT) {
      // default setting: do nothing
    } else if (cfg.i2s_format == I2S_LEFT_JUSTIFIED_FORMAT ||
               cfg.i2s_format == I2S_LSB_FORMAT) {
      if (!i2s.setLSBJFormat()) {
        LOGE("Could not set LSB Format")
        return false;
      }
    } else {
      LOGE("Unsupported I2S format");
      return false;
    }

#if defined(RP2040_HOWER)

    if (cfg.signal_type != TDM && (cfg.channels < 1 || cfg.channels > 2)) {
      LOGE("Unsupported channels: '%d'", cfg.channels);
      return false;
    }

    if (cfg.signal_type == TDM) {
      i2s.setTDMFormat();
      i2s.setTDMChannels(cfg.channels);
    }

#else

    if (cfg.channels < 1 || cfg.channels > 2) {
      LOGE("Unsupported channels: '%d'", cfg.channels);
      return false;
    }

    if (cfg.signal_type != Digital) {
      LOGE("Unsupported signal_type: '%d'", cfg.signal_type);
      return false;
    }

#endif
    if (!i2s.begin(cfg.sample_rate)) {
      LOGE("Could not start I2S");
      return false;
    }
    is_active = true;
    return true;
  }

  /// stops the I2C and uninstalls the driver
  void end() {
    flush();
    i2s.end();
    is_active = false;
  }

  /// provides the actual configuration
  I2SConfigStd config() { return cfg; }

  /// writes the data to the I2S interface
  size_t writeBytes(const void *src, size_t size_bytes) {
    LOGD("writeBytes(%d)", size_bytes);
    size_t result = 0;

    if (cfg.channels == 1) {
      result = writeExpandChannel(src, size_bytes);
    } else if (cfg.channels == 2) {
      const uint8_t *p = (const uint8_t *)src;
      while (size_bytes >= sizeof(int32_t)) {
        bool justWritten = i2s.write(
            *(int32_t *)p,
            true);  // I2S::write(int32,bool) actually only returns 0 or 1
        if (justWritten) {
          size_bytes -= sizeof(int32_t);
          p += sizeof(int32_t);
          result += sizeof(int32_t);
        } else
          return result;
      }
    }
    return result;
  }

  size_t readBytes(void *dest, size_t size_bytes) {
    TRACED();
    switch (cfg.channels) {
      case 1:
        return read1Channel(dest, size_bytes);
      case 2:
        return read2Channels(dest, size_bytes);
    }
    return 0;
  }

  int availableForWrite() {
    if (cfg.channels == 1) {
      return cfg.buffer_size;
    } else {
      return i2s.availableForWrite();
    }
  }

  int available() { return min(i2s.available(), cfg.buffer_size); }

  void flush() { i2s.flush(); }

  bool getOverUnderflow() {
    return i2s.getOverUnderflow() ;
  }

 protected:
  I2SConfigStd cfg;
  I2S i2s;
  bool has_input[2];
  bool is_active = false;

  /// writes 1 channel to I2S while expanding it to 2 channels
  // returns amount of bytes written from src to i2s
  size_t writeExpandChannel(const void *src, size_t size_bytes) {
    switch (cfg.bits_per_sample) {
      // case 8: {
      //   int8_t *pt8 = (int8_t*) src;
      //   int16_t sample16 = static_cast<int16_t>(*pt8) << 8;
      //   for (int j=0;j<size_bytes;j++){
      //     // 8 bit does not work
      //     i2s.write8(pt8[j], pt8[j]);
      //     //LOGI("%d", pt8[j]);
      //   }
      // } break;
      case 16: {
        int16_t *pt16 = (int16_t *)src;
        for (int j = 0; j < size_bytes / sizeof(int16_t); j++) {
          i2s.write16(pt16[j], pt16[j]);
        }
      } break;
      case 24: {
        int32_t *pt24 = (int32_t *)src;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j++) {
          i2s.write24(pt24[j], pt24[j]);
        }
      } break;
      case 32: {
        int32_t *pt32 = (int32_t *)src;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j++) {
          i2s.write32(pt32[j], pt32[j]);
        }
      } break;
    }
    return size_bytes;
  }

  /// Provides sterio data from i2s
  size_t read2Channels(void *dest, size_t size_bytes) {
    TRACED();
    size_t result = 0;
    switch (cfg.bits_per_sample) {
        // case 8:{
        //   int8_t *data = (int8_t*)dest;
        //   for (int j=0;j<size_bytes;j+=2){
        //     if (i2s.read8(data+j, data+j+1)){
        //       result+=2;;
        //     } else {
        //       return result;
        //     }
        //   }
        // }break;

      case 16: {
        int16_t *data = (int16_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int16_t); j += 2) {
          if (i2s.read16(data + j, data + j + 1)) {
            result += 4;
          } else {
            return result;
          }
        }
      } break;

      case 24: {
        int32_t *data = (int32_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j += 2) {
          if (i2s.read24(data + j, data + j + 1)) {
            result += 8;
          } else {
            return result;
          }
        }
      } break;

      case 32: {
        int32_t *data = (int32_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j += 2) {
          if (i2s.read32(data + j, data + j + 1)) {
            result += 8;
          } else {
            return result;
          }
        }

      } break;
    }
    return result;
  }

  /// Reads 2 channels from i2s and combineds them to 1
  size_t read1Channel(void *dest, size_t size_bytes) {
    TRACED();
    size_t result = 0;
    switch (cfg.bits_per_sample) {
        // case 8:{
        //   int8_t tmp[2];
        //   int8_t *data = (int8_t*)dest;
        //   for (int j=0;j<size_bytes;j++){
        //     if (i2s.read8(tmp, tmp+1)){
        //       data[j] = mix(tmp[0], tmp[1]);
        //       result++;;
        //     } else {
        //       return result;
        //     }
        //   }
        // }break;

      case 16: {
        int16_t tmp[2];
        int16_t *data = (int16_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int16_t); j++) {
          if (i2s.read16(tmp, tmp + 1)) {
            data[j] = mix(tmp[0], tmp[1]);
            result += 2;
          } else {
            return result;
          }
        }
      } break;

      case 24: {
        int32_t tmp[2];
        int32_t *data = (int32_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j++) {
          if (i2s.read24(tmp, tmp + 1)) {
            data[j] = mix(tmp[0], tmp[1]);
            result += 4;
          } else {
            return result;
          }
        }
      } break;

      case 32: {
        int32_t tmp[2];
        int32_t *data = (int32_t *)dest;
        for (int j = 0; j < size_bytes / sizeof(int32_t); j++) {
          if (i2s.read32(tmp, tmp + 1)) {
            data[j] = mix(tmp[0], tmp[1]);
            result += 4;
          } else {
            return result;
          }
        }
      } break;
    }
    return result;
  }

  // we just provide the avg of both samples
  template <class T>
  T mix(T left, T right) {
    if (left != 0) has_input[0] = true;
    if (right != 0) has_input[1] = true;

    // if right is always empty we return left
    if (has_input[0] && !has_input[1]) {
      return left;
    }

    // if left is always empty we return right
    if (!has_input[0] && has_input[1]) {
      return right;
    }

    return (left / 2) + (right / 2);
  }
};

using I2SDriver = I2SDriverRP2040;

}  // namespace audio_tools

#endif
