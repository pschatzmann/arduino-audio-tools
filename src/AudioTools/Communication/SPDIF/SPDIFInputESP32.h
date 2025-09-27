
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>

#include "SPDIFHistogram.h"
#include "driver/rmt_rx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace audio_tools {

/**
 * @class SPDIFInputESP32
 * @brief S/PDIF decoder for ESP32 using RMT and FreeRTOS.
 *
 * This class decodes S/PDIF audio streams using the ESP32 RMT peripheral and
 * FreeRTOS tasks. It collects symbol durations, builds pulse histograms,
 * analyzes timing, and extracts PCM samples.
 *
 * Features:
 * - Instance-based design for multiple decoder support
 * - Automatic channel detection (mono/stereo)
 * - Sample rate detection
 * - PCM output via ring buffer
 *
 * Usage:
 * 1. Construct with input pin
 * 2. Call begin() to start decoding
 * 3. Use readBytes() to retrieve PCM data
 * 4. Use audioInfo() for stream metadata
 *
 * @author Nathan Ladwig (netham45)
 * @author Phil Schatzmann (pschatzmann)
 */
class SPDIFInputESP32 : public AudioStream {

 public:
  /**
   * @brief Constructor: initializes S/PDIF input on the given pin
   * @param input_pin GPIO pin for S/PDIF input
   */
  SPDIFInputESP32(int input_pin) { init(input_pin); }

  /**
   * @brief Destructor. Cleans up resources.
   */
  /**
   * @brief Destructor: cleans up resources
   */
  ~SPDIFInputESP32() { deinit(); }

  /**
   * @brief Starts the decoder task and clears channel tracking
   * @return true if task started successfully, false otherwise
   */
  bool begin(void) {
    channels_seen.clear();
    // Setup PCM ring buffer
    pcm_buffer = xRingbufferCreate(PCM_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!pcm_buffer) {
      return false;
    }
    if (xTaskCreatePinnedToCore(spdifDecoderTaskCallback, "spdif_decoder",
                                DECODER_TASK_STACK, this, DECODER_TASK_PRIORITY,
                                &g_decoder_task, 1) != pdPASS) {
      vRingbufferDelete(pcm_buffer);
      pcm_buffer = nullptr;
      return false;
    }
    return true;
  }

  /**
   * @brief Disables the S/PDIF receiver
   * @return ESP_OK on success
   */
  void end(void) {
    if (g_rx_channel) {
       rmt_disable(g_rx_channel);
    }
  }

  /**
   * @brief Reads PCM bytes from the buffer
   * @param buffer Output buffer
   * @param size Number of bytes to read
   * @return Number of bytes read
   */
  size_t readBytes(uint8_t* buffer, size_t size) {
    if (!pcm_buffer) {
      return 0;
    }
    size_t received_size = 0;
    uint8_t* data = (uint8_t*)xRingbufferReceiveUpTo(pcm_buffer, &received_size,
                                                     pdMS_TO_TICKS(10), size);
    if (data && received_size > 0) {
      memcpy(buffer, data, received_size);
      vRingbufferReturnItem(pcm_buffer, (void*)data);
      return received_size;
    }
    return 0;
  }

 protected:
  // Configuration constants
  static constexpr uint32_t RMT_RESOLUTION_HZ = 80000000;
  static constexpr uint32_t RMT_MEM_BLOCK_SYMBOLS = 8192;
  static constexpr uint32_t SYMBOL_BUFFER_SIZE = 8192;
  static constexpr uint32_t PCM_BUFFER_SIZE = 4096;
  static constexpr uint32_t DECODER_TASK_STACK = 4096;
  static constexpr uint32_t DECODER_TASK_PRIORITY = 10;
  static constexpr uint32_t MIN_SAMPLES_FOR_ANALYSIS = 10000;
  static constexpr uint32_t TIMING_VARIANCE = 3;

  // Preamble patterns
  static constexpr uint8_t PREAMBLE_B_0 = 0xE8;
  static constexpr uint8_t PREAMBLE_B_1 = 0x17;
  static constexpr uint8_t PREAMBLE_M_0 = 0xE2;
  static constexpr uint8_t PREAMBLE_M_1 = 0x1D;
  static constexpr uint8_t PREAMBLE_W_0 = 0xE4;
  static constexpr uint8_t PREAMBLE_W_1 = 0x1B;
  
  // The decoder state struct should be defined as a member
  struct DecoderState {
    uint32_t state = 0;
    uint32_t bit_count = 0;
    uint32_t subframe_data = 0;
    uint32_t preamble_data = 0;
    uint32_t channel = 0;
    int16_t left_sample = 0;
  } ds;

  RingbufHandle_t g_symbol_buffer = nullptr;
  RingbufHandle_t pcm_buffer = nullptr;
  SPDIFHistogram histogram;
  uint8_t lut[256] = {0};
  // 256-byte LUT for pulse classification
  uint8_t pulse_lut[256] = {0};
  TaskHandle_t g_decoder_task = nullptr;
  rmt_channel_handle_t g_rx_channel = nullptr;
  rmt_receive_config_t g_rx_config;
  rmt_symbol_word_t* g_rmt_buffer = nullptr;
  // Track detected channels
  std::set<uint32_t> channels_seen;

  /**
   * @brief Returns detected sample rate (Hz)
   * @return Sample rate in Hz
   */
  uint32_t getSampleRate(void) {
    auto timing = histogram.getTiming();
    if (!timing.timing_discovered) {
      return 0;
    }

    uint32_t T = timing.base_unit_ticks;

    switch (T) {
      case 13:
        return 48000;
      case 14:
        return 44100;
      default:
        return 0;
    }
  }

  /**
   * @brief Returns the number of unique channels detected
   * @return Number of channels
   */
  size_t getNumChannels() const { return channels_seen.size(); }

  /**
   * @brief Main decoder FreeRTOS task
   * @param arg Pointer to SPDIFInputESP32 instance
   */
  static void spdifDecoderTaskCallback(void* arg) {
    SPDIFInputESP32* self = static_cast<SPDIFInputESP32*>(arg);
    rmt_receive_config_t& rxConfig = self->g_rx_config;
    rmt_channel_handle_t& rxChannel = self->g_rx_channel;
    SPDIFHistogram& histogram = self->histogram;
    RingbufHandle_t& symbolBuffer = self->g_symbol_buffer;
    RingbufHandle_t& pcmBuffer = self->pcm_buffer;
    rmt_symbol_word_t* rmtBuffer = self->g_rmt_buffer;

    rxConfig.signal_range_min_ns = 10;
    rxConfig.signal_range_max_ns = 10000;
    rxConfig.flags.en_partial_rx = true;

    ESP_ERROR_CHECK(rmt_enable(rxChannel));
    ESP_ERROR_CHECK(rmt_receive(
        rxChannel, rmtBuffer, RMT_MEM_BLOCK_SYMBOLS * sizeof(rmt_symbol_word_t),
        &rxConfig));

    size_t rxSize = 0;
    rmt_symbol_word_t* symbols = nullptr;

    LOGI("Decoder task started, waiting for PCM buffer");
    while (!pcmBuffer) vTaskDelay(100);
    LOGI("PCM buffer found, continuing");
    while (1) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      symbols =
          (rmt_symbol_word_t*)xRingbufferReceive(symbolBuffer, &rxSize, 0);
      while (symbols) {
        auto timing = histogram.getTiming();
        size_t numSymbols = rxSize / sizeof(rmt_symbol_word_t);
        if (!timing.timing_discovered) {
          // Convert rmt_symbol_word_t durations to uint32_t array for histogram
          std::vector<uint32_t> durations;
          durations.reserve(numSymbols * 2);
          for (size_t i = 0; i < numSymbols; ++i) {
            durations.push_back(symbols[i].duration0);
            durations.push_back(symbols[i].duration1);
          }
          histogram.collectPulseHistogram(durations.data(), durations.size());
          if (timing.total_samples >= MIN_SAMPLES_FOR_ANALYSIS) {
            histogram.analyzePulseTiming();
          }
        } else {
          if (!self->pulse_lut[0]) {
            self->decoderInitThresholds(timing);
          }
          // Instance state
          for (size_t i = 0; i < numSymbols; i++) {
            self->processSymbol(symbols[i].duration0);
            self->processSymbol(symbols[i].duration1);
          }
        }

        // --- Detect changes in sample rate or channel count ---
        uint32_t new_sample_rate = self->getSampleRate();
        uint16_t new_num_channels = self->getNumChannels();
        if (new_sample_rate != self->info.sample_rate || new_num_channels != self->info.channels) {
          ESP_LOGI("SPDIFInputESP32", "Stream format changed: sample_rate=%u, channels=%u", new_sample_rate, (unsigned)new_num_channels);
          AudioInfo info{new_sample_rate, new_num_channels, 16}; 
          self->setAudioInfo(info);
        }

        vRingbufferReturnItem(symbolBuffer, (void*)symbols);
        symbols =
            (rmt_symbol_word_t*)xRingbufferReceive(symbolBuffer, &rxSize, 0);
      }
    }
  }

  /**
   * @brief RMT receive ISR callback
   * @param channel RMT channel handle
   * @param edata RMT RX done event data
   * @param user_ctx User context (SPDIFInputESP32 instance)
   * @return true if task notification was given
   */
  static bool IRAM_ATTR rmtRxDoneCallback(rmt_channel_handle_t channel,
                                          const rmt_rx_done_event_data_t* edata,
                                          void* user_ctx) {
    SPDIFInputESP32* self = static_cast<SPDIFInputESP32*>(user_ctx);
    BaseType_t taskWoken = pdFALSE;

    if (edata->flags.is_last) {
      rmt_receive(self->g_rx_channel, self->g_rmt_buffer,
                  RMT_MEM_BLOCK_SYMBOLS * sizeof(rmt_symbol_word_t),
                  &self->g_rx_config);
    }

    if (edata->num_symbols > 0) {
      xRingbufferSendFromISR(self->g_symbol_buffer, edata->received_symbols,
                             edata->num_symbols * sizeof(rmt_symbol_word_t),
                             &taskWoken);
    }

    vTaskNotifyGiveFromISR(self->g_decoder_task, &taskWoken);
    return taskWoken == pdTRUE;
  }

  /**
   * @brief Initializes S/PDIF receiver hardware
   * @param input_pin GPIO pin for S/PDIF input
   * @return ESP_OK on success
   */
  esp_err_t init(int input_pin) {
    LOGI("SPDIF");

    g_symbol_buffer =
        xRingbufferCreate(SYMBOL_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!g_symbol_buffer) {
      return ESP_FAIL;
    }

    g_rmt_buffer = (rmt_symbol_word_t*)heap_caps_malloc(
        RMT_MEM_BLOCK_SYMBOLS * sizeof(rmt_symbol_word_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!g_rmt_buffer) {
      return ESP_FAIL;
    }
    rmt_rx_channel_config_t rx_channel_cfg;
    rx_channel_cfg.gpio_num = static_cast<gpio_num_t>(input_pin);
    rx_channel_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    rx_channel_cfg.resolution_hz = RMT_RESOLUTION_HZ;
    rx_channel_cfg.mem_block_symbols = RMT_MEM_BLOCK_SYMBOLS;
    rx_channel_cfg.flags.with_dma = true;

    esp_err_t ret = rmt_new_rx_channel(&rx_channel_cfg, &g_rx_channel);
    if (ret != ESP_OK) {
      return ESP_FAIL;
    }

    rmt_rx_event_callbacks_t cbs = {.on_recv_done = rmtRxDoneCallback};
    ret = rmt_rx_register_event_callbacks(g_rx_channel, &cbs, this);

    if (ret != ESP_OK) {
      return ESP_FAIL;
    }

    return ESP_OK;
  }

  /**
   * @brief Cleans up and releases all resources
   */
  void deinit(void) {
    if (g_rx_channel) {
      rmt_disable(g_rx_channel);
      rmt_del_channel(g_rx_channel);
      g_rx_channel = nullptr;
    }
    if (g_rmt_buffer) {
      heap_caps_free(g_rmt_buffer);
      g_rmt_buffer = nullptr;
    }
    if (g_symbol_buffer) {
      vRingbufferDelete(g_symbol_buffer);
      g_symbol_buffer = nullptr;
    }
    if (pcm_buffer) {
      vRingbufferDelete(pcm_buffer);
      pcm_buffer = nullptr;
    }
  }

  /**
   * @brief Initializes the pulse classification lookup table using timing
   * thresholds.
   * @param timing Timing thresholds from histogram analysis.
   */
  /**
   * @brief Initializes LUT for pulse classification using timing
   * @param timing Timing thresholds from histogram analysis
   */
  inline void decoderInitThresholds(const SPDIFHistogram::Timing& timing) {
    for (int i = 0; i < 256; i++) {
      if (i < timing.short_pulse_ticks - TIMING_VARIANCE) {
        lut[i] = 3;  // UNKNOWN
      } else if (i < timing.short_pulse_ticks + TIMING_VARIANCE) {
        lut[i] = 0;  // SHORT
      } else if (i < timing.medium_pulse_ticks - TIMING_VARIANCE) {
        lut[i] = 3;  // UNKNOWN
      } else if (i < timing.medium_pulse_ticks + TIMING_VARIANCE) {
        lut[i] = 1;  // MEDIUM
      } else if (i < timing.long_pulse_ticks - TIMING_VARIANCE) {
        lut[i] = 3;  // UNKNOWN
      } else if (i < timing.long_pulse_ticks + TIMING_VARIANCE) {
        lut[i] = 2;  // LONG
      } else {
        lut[i] = 3;  // UNKNOWN
      }
    }
  }


  /**
   * @brief Decodes a single symbol and updates decoder state
   * @param dur Symbol duration value
   * @param stereoOut Optional output for stereo sample
   */
  inline void processSymbol(uint32_t dur) {
    // Lookup pulse type from LUT (0=short, 1=medium, 2=long, 3=unknown)
    uint32_t ptype = lut[dur & 0xFF];
    if (ptype < 3) {
      // --- PREAMBLE HANDLING ---
      // If currently in a preamble sequence
      if (ds.state & 2) {
        ds.state ^= 8;  // Toggle signal level (used for bit decoding)
        uint32_t bits_to_add =
            ptype + 1;  // Number of bits to add (short=1, medium=2, long=3)
        uint32_t pindex =
            (ds.preamble_data >> 8) & 0xF;  // Current bit index in preamble
        uint32_t pattern = ds.preamble_data & 0xFF;  // Current preamble pattern
        // Add bits to preamble pattern
        for (uint32_t j = 0; j < bits_to_add && pindex < 8; j++) {
          if (ds.state & 8)
            pattern |= (1 << (7 - pindex));  // Set bit if level is high
          pindex++;
        }
        // If preamble pattern is complete (8 bits)
        if (pindex >= 8) {
          ds.state &= ~2;  // Exit preamble mode
          // Identify channel from preamble pattern
          if (pattern == PREAMBLE_B_0 || pattern == PREAMBLE_B_1) {
            ds.channel = 0;  // Left channel
          } else if (pattern == PREAMBLE_M_0 || pattern == PREAMBLE_M_1) {
            ds.channel = 0;  // Left channel (alternate)
          } else if (pattern == PREAMBLE_W_0 || pattern == PREAMBLE_W_1) {
            ds.channel = 1;  // Right channel
          }
          // Track channel
          channels_seen.insert(ds.channel);
        } else {
          ds.preamble_data = pattern | (pindex << 8);  // Update preamble state
        }
      }
      // --- PREAMBLE DETECTION ---
      // Detect start of new preamble (long pulse, not expecting short)
      else if (ptype == 2 && !(ds.state & 1)) {
        ds.state |= 2;  // Enter preamble mode
        ds.preamble_data = 0;
        ds.state =
            (ds.state & ~8) | ((ds.state & 4) << 1);  // Set initial level
        ds.state ^= 8;                                // Toggle level
        uint32_t pattern = 0;
        if (ds.state & 8) pattern = 0xE0;  // Set initial bits if level is high
        ds.preamble_data = pattern | (3 << 8);  // Start with 3 bits in preamble
        ds.bit_count = 0;
        ds.subframe_data = 0;
        ds.state &= ~1;  // Clear expecting_short
      }
      // --- DATA BIT DECODING ---
      // If not in preamble, decode data bits
      else if (ds.bit_count < 28) {
        // If expecting a short pulse (bit value)
        if (ds.state & 1) {
          if (ptype == 0) {
            ds.subframe_data |=
                (1UL << ds.bit_count);  // Set bit for short pulse
          }
          ds.bit_count++;
          ds.state &= ~1;  // Clear expecting_short
        } else {
          // Medium pulse: just increment bit count
          if (ptype == 1) {
            ds.bit_count++;
          } else if (ptype == 0) {
            ds.state |= 1;  // Next pulse should be short
          }
        }
        // If all 28 bits decoded, extract sample
        if (ds.bit_count == 28) {
          ds.state =
              (ds.state & ~4) |
              ((ds.subframe_data & (1UL << 27)) ? 4 : 0);  // Set last level
          int32_t sample =
              (int32_t)(ds.subframe_data & 0xFFFFFF);  // 24-bit sample
          // Sign-extend if needed
          if (sample & 0x800000) {
            sample |= 0xFF000000;
          }
          int16_t s16 = (int16_t)(sample >> 8);  // Convert to 16-bit
          if (ds.channel == 0) {
            ds.left_sample = s16;  // Store left channel sample
          }
        }
      }
    }
  }
};

}  // namespace audio_tools
