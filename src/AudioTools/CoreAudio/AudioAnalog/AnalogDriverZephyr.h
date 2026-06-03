#pragma once

#include "AudioToolsConfig.h"
#if defined(USE_ANALOG) && \
    ( defined(IS_ZEPHYR) || defined(DOXYGEN))
#include <limits.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/dac.h>

#include "AudioTools/CoreAudio/AudioAnalog/AnalogDriverBase.h"
#include "AudioTools/CoreAudio/AudioTimer/AudioTimer.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

class AnalogDriverZephyr;
using AnalogDriver = AnalogDriverZephyr;

/**
 * @brief Zephyr analog driver for timer-driven DAC/ADC streaming.
 * @ingroup platform
 *
 * This driver provides a simple internal-analog implementation for Zephyr:
 * - TX mode: PCM samples are buffered and written to DAC at `sample_rate`.
 * - RX mode: ADC is sampled at `sample_rate` and converted to PCM bytes.
 *
 * The timer callback moves exactly one audio frame per tick:
 * $$\text{frame\_size} = \text{channels} \times
 * \frac{\text{bits\_per\_sample}}{8}$$
 *
 * Mode behavior:
 * - `TX_MODE`: `write()` accepts data, `readBytes()` returns 0.
 * - `RX_MODE`: `readBytes()` returns captured data, `write()` returns 0.
 * - `RXTX_MODE`: not supported.
 *
 * Channel mapping:
 * - Logical channel index -> hardware channel ID is resolved via
 *   `mapOutputChannelId()` / `mapInputChannelId()`.
 * - Default mapping uses `config.pins()[index]` when provided, otherwise
 * `index`.
 *
 * Notes:
 * - Designed for portability and simplicity, not maximum throughput.
 * - TX underflow outputs mid-scale silence.
 * - RX samples are centered/scaled to signed PCM representation.
 */
class AnalogDriverZephyr : public AnalogDriverBase {
 public:
  AnalogDriverZephyr() = default;
  virtual ~AnalogDriverZephyr() { end(); }

  /**
   * @brief Initializes driver resources for the selected mode.
   *
   * Performs mode-specific device resolution and channel setup, creates the
   * ring buffer, and starts periodic timer processing.
   */
  bool begin(AnalogConfig cfg) override {
    TRACED();
    end();

    config = cfg;
    frame_size = config.channels * (config.bits_per_sample / 8);
    if (frame_size <= 0 || config.channels <= 0) {
      LOGE("Invalid audio format");
      return false;
    }

    if (!setupBuffer()) {
      LOGE("Analog buffer setup failed");
      return false;
    }

#if IS_ENABLED(CONFIG_DAC)
    if (config.rx_tx_mode == TX_MODE) {
      if (!resolveDacDevice()) {
        LOGE("No DAC device found");
        return false;
      }
      if (!setupDacChannels()) {
        LOGE("DAC channel setup failed");
        return false;
      }
    } else
#endif
#if IS_ENABLED(CONFIG_ADC)
        if (config.rx_tx_mode == RX_MODE) {
      if (!resolveAdcDevice()) {
        LOGE("No ADC device found");
        return false;
      }
      if (!setupAdcChannels()) {
        LOGE("ADC channel setup failed");
        return false;
      }
    } else
#endif
    {
      LOGE("RXTX_MODE is not supported by AnalogDriverZephyr");
      return false;
    }

    if (!setupTimer()) {
      LOGE("Analog timer setup failed");
      return false;
    }

    active = true;
    return true;
  }

  /// Stops timer processing and releases buffered resources.
  void end() override {
    timer.end();
    active = false;
    deleteBuffer();
  }

  /// Queues TX PCM bytes into the ring buffer (TX mode only).
  size_t write(const uint8_t* src, size_t size_bytes) override {
    if (config.rx_tx_mode != TX_MODE) return 0;
    if (!active || src == nullptr || size_bytes == 0) return 0;

    size_t bytes = (size_bytes / frame_size) * frame_size;
    if (bytes == 0) return 0;
    if (buffer == nullptr) return 0;

    if (config.is_blocking_write) {
      int wait_ms = 0;
      while (active && buffer->availableForWrite() < bytes) {
        if (++wait_ms > ANALOG_ZEPHYR_WRITE_TIMEOUT_MS) {
          LOGW("write timeout: requested=%u available=%u", (unsigned)bytes,
               (unsigned)buffer->availableForWrite());
          break;
        }
        delay(1);
      }
    }
    size_t result = buffer->writeArray(src, bytes);
    if (result != bytes) {
      LOGW("partial write: %u -> %u", (unsigned)bytes, (unsigned)result);
    }
    return result;
  }

  /// Reads captured RX PCM bytes from the ring buffer (RX mode only).
  size_t readBytes(uint8_t* dest, size_t size_bytes) override {
    if (config.rx_tx_mode != RX_MODE) return 0;
    if (!active || dest == nullptr || size_bytes == 0 || buffer == nullptr)
      return 0;
    size_t bytes = (size_bytes / frame_size) * frame_size;
    if (bytes == 0) return 0;
    return buffer->readArray(dest, bytes);
  }

  /// Number of bytes currently available for reading (RX mode).
  int available() override {
    if (config.rx_tx_mode != RX_MODE || buffer == nullptr) return 0;
    return buffer->available();
  }

  /// Number of bytes currently available for writing (TX mode).
  int availableForWrite() override {
    if (config.rx_tx_mode != TX_MODE) return 0;
    if (buffer == nullptr) return 0;
    if (config.is_blocking_write) return config.buffer_size;
    return buffer->availableForWrite() / frame_size * frame_size;
  }

 private:
  AnalogConfig config;
  const struct device* dac_dev = nullptr;
  const struct device* adc_dev = nullptr;
  TimerAlarmRepeating timer;
  BaseBuffer<uint8_t>* buffer = nullptr;
  bool active = false;
  int frame_size = 0;
  int configured_channels = 0;
  uint8_t channel_ids[2] = {0, 1};
  uint8_t adc_resolution_bits = 12;
  static constexpr int ANALOG_ZEPHYR_WRITE_TIMEOUT_MS = 1000;

  /// Allocates the internal ring buffer if needed.
  bool setupBuffer() {
    if (buffer != nullptr) return true;
    int size = config.buffer_count * config.buffer_size;
    buffer = new RingBuffer<uint8_t>(size);
    return buffer != nullptr;
  }

  /// Releases the internal ring buffer.
  void deleteBuffer() {
    if (buffer != nullptr) {
      delete buffer;
      buffer = nullptr;
    }
  }

  /// Configures periodic processing at `config.sample_rate`.
  bool setupTimer() {
    timer.setCallbackParameter(this);
    timer.setIsSave(true);
    return timer.begin(timerCallback, config.sample_rate, HZ);
  }

  /// Writes a mid-scale frame to DAC (used on TX underflow).
  void writeSilenceFrame() {
#if IS_ENABLED(CONFIG_DAC)
    const uint32_t mid = (1u << dacResolution()) / 2u;
    for (int ch = 0; ch < configured_channels; ++ch) {
      int rc = dac_write_value(dac_dev, channel_ids[ch], mid);
      if (rc != 0) {
        LOGW("dac_write_value(silence) failed: ch=%d rc=%d", ch, rc);
      }
    }
#endif
  }

  /// Consumes one frame from TX buffer and writes it to DAC.
  void outputOneFrame() {
#if IS_ENABLED(CONFIG_DAC)
    if (!active || buffer == nullptr) return;
    if (buffer->available() < frame_size) {
      writeSilenceFrame();
      return;
    }

    const int bps = config.bits_per_sample;
    const size_t sample_size = bps / 8;
    uint8_t raw[4] = {0, 0, 0, 0};

    for (int ch = 0; ch < config.channels; ++ch) {
      for (size_t b = 0; b < sample_size; ++b) {
        uint8_t v = 0;
        buffer->read(v);
        raw[b] = v;
      }
      if (ch < configured_channels) {
        int32_t sample = readSample(raw, bps);
        uint32_t value = toDacValue(sample, bps);
        int rc = dac_write_value(dac_dev, channel_ids[ch], value);
        if (rc != 0) {
          LOGW("dac_write_value failed: ch=%d rc=%d", ch, rc);
        }
      }
    }
#endif
  }

  /// Samples one RX frame from ADC and appends it to the RX buffer.
  void sampleOneFrame() {
#if IS_ENABLED(CONFIG_ADC)
    if (!active || buffer == nullptr) return;
    if (buffer->availableForWrite() < frame_size) return;

    const int bps = config.bits_per_sample;
    for (int ch = 0; ch < configured_channels; ++ch) {
      int16_t raw = 0;
      if (!readAdcChannel(channel_ids[ch], raw)) {
        raw = 0;
      }

      int32_t s = adcRawToSigned(raw);
      writeSampleToBuffer(s, bps);
    }

    // if channels requested exceed configured ADC channels: duplicate first
    for (int ch = configured_channels; ch < config.channels; ++ch) {
      int32_t s = 0;
      if (configured_channels > 0) {
        int16_t raw = 0;
        if (readAdcChannel(channel_ids[0], raw)) {
          s = adcRawToSigned(raw);
        }
      }
      writeSampleToBuffer(s, bps);
    }
#endif
  }

  /// Timer callback dispatches one-frame TX or RX processing.
  static void timerCallback(void* arg) {
    auto* self = static_cast<AnalogDriverZephyr*>(arg);
    if (self == nullptr) return;
    if (self->config.rx_tx_mode == TX_MODE) {
      self->outputOneFrame();
    } else {
      self->sampleOneFrame();
    }
  }

  /// Resolves DAC device using common Zephyr device names.
  bool resolveDacDevice() {
#if IS_ENABLED(CONFIG_DAC)
    if (dac_dev != nullptr && device_is_ready(dac_dev)) return true;

    static const char* names[] = {
        "dac0",
        "DAC_0",
    };
    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); ++j) {
      const struct device* dev = device_get_binding(names[j]);
      if (dev != nullptr && device_is_ready(dev)) {
        dac_dev = dev;
        LOGI("DAC device: %s", names[j]);
        return true;
      }
    }
    return false;
#else
    return false;
#endif
  }

  /// Resolves ADC device using common Zephyr device names.
  bool resolveAdcDevice() {
#if IS_ENABLED(CONFIG_ADC)
    if (adc_dev != nullptr && device_is_ready(adc_dev)) return true;

    static const char* names[] = {
        "adc0",
        "ADC_0",
        "adc",
    };
    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); ++j) {
      const struct device* dev = device_get_binding(names[j]);
      if (dev != nullptr && device_is_ready(dev)) {
        adc_dev = dev;
        LOGI("ADC device: %s", names[j]);
        return true;
      }
    }
    return false;
#else
    return false;
#endif
  }

  /// Applies DAC channel configuration and stores mapped channel IDs.
  bool setupDacChannels() {
#if IS_ENABLED(CONFIG_DAC)
    configured_channels = config.channels <= 2 ? config.channels : 2;
    if (configured_channels <= 0) return false;

    for (int ch = 0; ch < configured_channels; ++ch) {
      uint8_t channel = mapOutputChannelId(ch);

      struct dac_channel_cfg cfg = {};
      cfg.channel_id = channel;
      cfg.resolution = dacResolution();
#if defined(CONFIG_DAC_BUFFERED)
      cfg.buffered = true;
#endif
      int rc = dac_channel_setup(dac_dev, &cfg);
      if (rc != 0) {
        LOGE("dac_channel_setup failed: ch=%d rc=%d", (int)channel, rc);
        return false;
      }
      channel_ids[ch] = channel;
    }
    return true;
#else
    return false;
#endif
  }

  /// Applies ADC channel configuration and stores mapped channel IDs.
  bool setupAdcChannels() {
#if IS_ENABLED(CONFIG_ADC)
    configured_channels = config.channels <= 2 ? config.channels : 2;
    if (configured_channels <= 0) return false;

    adc_resolution_bits = adcResolution();

    for (int ch = 0; ch < configured_channels; ++ch) {
      uint8_t channel = mapInputChannelId(ch);

      struct adc_channel_cfg cfg = {};
      cfg.gain = ADC_GAIN_1;
      cfg.reference = ADC_REF_INTERNAL;
      cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
      cfg.channel_id = channel;
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
      cfg.input_positive = channel;
#endif

      int rc = adc_channel_setup(adc_dev, &cfg);
      if (rc != 0) {
        LOGE("adc_channel_setup failed: ch=%d rc=%d", (int)channel, rc);
        return false;
      }
      channel_ids[ch] = channel;
    }
    return true;
#else
    return false;
#endif
  }

  /// Selects DAC resolution used for TX scaling.
  uint8_t dacResolution() const {
    if (config.bits_per_sample >= 12) return 12;
    if (config.bits_per_sample >= 10) return 10;
    return 8;
  }

  /// Selects ADC resolution used for RX sampling.
  uint8_t adcResolution() const {
    if (config.bits_per_sample >= 12) return 12;
    if (config.bits_per_sample >= 10) return 10;
    if (config.bits_per_sample >= 8) return 8;
    return 10;
  }

  /// Maps logical output channel index (0..n) to Zephyr DAC channel ID.
  /// Default behavior:
  /// 1) Use configured pins()[index] if provided and >= 0
  /// 2) Fallback to index
  uint8_t mapOutputChannelId(int index) {
    Pins& pins = config.pins();
    if (pins.size() > index && pins[index] >= 0) {
      return static_cast<uint8_t>(pins[index]);
    }
    return static_cast<uint8_t>(index);
  }

  /// Maps logical input channel index (0..n) to Zephyr ADC channel ID.
  /// Default behavior:
  /// 1) Use configured pins()[index] if provided and >= 0
  /// 2) Fallback to index
  uint8_t mapInputChannelId(int index) {
    Pins& pins = config.pins();
    if (pins.size() > index && pins[index] >= 0) {
      return static_cast<uint8_t>(pins[index]);
    }
    return static_cast<uint8_t>(index);
  }

  /// Decodes a signed PCM sample from little-endian byte storage.
  static int32_t readSample(const uint8_t* p, int bps) {
    switch (bps) {
      case 8:
        return (int32_t)(*((const int8_t*)p));
      case 16:
        return (int32_t)(*((const int16_t*)p));
      case 24: {
        int32_t v =
            ((int32_t)p[0]) | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
        if (v & 0x00800000) v |= 0xFF000000;
        return v;
      }
      default:
        return *((const int32_t*)p);
    }
  }

  /// Performs a single-shot ADC read for one configured channel.
  bool readAdcChannel(uint8_t channel, int16_t& out) {
#if IS_ENABLED(CONFIG_ADC)
    int16_t sample = 0;
    struct adc_sequence sequence = {};
    sequence.channels = BIT(channel);
    sequence.buffer = &sample;
    sequence.buffer_size = sizeof(sample);
    sequence.resolution = adc_resolution_bits;

    int rc = adc_read(adc_dev, &sequence);
    if (rc != 0) {
      return false;
    }
    out = sample;
    return true;
#else
    (void)channel;
    (void)out;
    return false;
#endif
  }

  /// Converts unsigned/raw ADC code to centered signed sample domain.
  int32_t adcRawToSigned(int16_t raw) const {
    const int32_t maxv = (1 << adc_resolution_bits) - 1;
    const int32_t mid = maxv / 2;
    int32_t centered = (int32_t)raw - mid;
    // scale to signed 16-bit baseline
    return centered * (32767 / (mid > 0 ? mid : 1));
  }

  /// Encodes one signed sample into buffer according to configured bit depth.
  void writeSampleToBuffer(int32_t sample, int bps) {
    switch (bps) {
      case 8: {
        int8_t v = (int8_t)(sample >> 8);
        if (!buffer->write((uint8_t)v)) {
          LOGW("RX buffer full (8-bit)");
        }
      } break;
      case 16: {
        int16_t v = (int16_t)sample;
        uint8_t* p = (uint8_t*)&v;
        if (!buffer->write(p[0]) || !buffer->write(p[1])) {
          LOGW("RX buffer full (16-bit)");
        }
      } break;
      case 24: {
        int32_t v = sample << 8;
        uint8_t* p = (uint8_t*)&v;
        if (!buffer->write(p[0]) || !buffer->write(p[1]) ||
            !buffer->write(p[2])) {
          LOGW("RX buffer full (24-bit)");
        }
      } break;
      default: {
        int32_t v = sample << 16;
        uint8_t* p = (uint8_t*)&v;
        if (!buffer->write(p[0]) || !buffer->write(p[1]) ||
            !buffer->write(p[2]) || !buffer->write(p[3])) {
          LOGW("RX buffer full (32-bit)");
        }
      } break;
    }
  }

  /// Scales signed PCM sample to DAC code range `[0 .. 2^N-1]`.
  uint32_t toDacValue(int32_t sample, int bps) const {
    int64_t min_in = 0;
    int64_t max_in = 0;
    switch (bps) {
      case 8:
        min_in = INT8_MIN;
        max_in = INT8_MAX;
        break;
      case 16:
        min_in = INT16_MIN;
        max_in = INT16_MAX;
        break;
      case 24:
        min_in = -8388608LL;
        max_in = 8388607LL;
        break;
      default:
        min_in = INT32_MIN;
        max_in = INT32_MAX;
        break;
    }

    const uint32_t max_out = (1u << dacResolution()) - 1u;
    int64_t num = (int64_t)(sample - min_in) * (int64_t)max_out;
    int64_t den = (max_in - min_in);
    int64_t v = den ? (num / den) : 0;
    if (v < 0) v = 0;
    if ((uint64_t)v > max_out) v = max_out;
    return (uint32_t)v;
  }
};

}  // namespace audio_tools

#endif
