#pragma once

#include "AudioToolsConfig.h"

#if (defined(ESP32) && defined(USE_ANALOG) &&  !USE_LEGACY_I2S) || defined(DOXYGEN)

#ifdef ARDUINO
    #ifndef perimanClearPinBus
        #define perimanClearPinBus(p) perimanSetPinBus(p, ESP32_BUS_TYPE_INIT, NULL)
    #endif
#endif

#include "AudioTools/CoreAudio/AudioAnalog/AnalogDriverBase.h"
#include "AudioTools/CoreAudio/AudioAnalog/AnalogConfigESP32V1.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/AudioStreamsConverter.h"

namespace audio_tools {

/**
 * @brief AnalogAudioStream: A very fast DAC using DMA using the new
 * dac_continuous API
 * @ingroup platform
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AnalogDriverESP32V1 : public AnalogDriverBase {
public:
    /// Default constructor
    AnalogDriverESP32V1() {}

    /// Destructor
    virtual ~AnalogDriverESP32V1() { 
        end(); 
    }

    /// Start the Analog driver
    /// ----------------------------------------------------------
    bool begin(AnalogConfigESP32V1 cfg) {
        TRACEI();
        bool result = true;
        this->cfg = cfg;

        switch (cfg.rx_tx_mode) {
            case TX_MODE:
                if (!setup_tx()) return false;
                // convert to 16 bits
                if (!converter.begin(cfg, 16)) {
                    LOGE("converter");
                    return false;
                }
                active_tx = true;
                break;
            case RX_MODE:
                if (!setup_rx()) return false;
                active_rx = true;
                break;
            default:
                LOGE( "Unsupported MODE: %d", cfg.rx_tx_mode);
                return false;
        }

        active = true;
        return active;
    }

    /// Stop and uninstalls the driver
    /// ----------------------------------------------------------
    void end() override {
        TRACEI();
        if (active_tx) {
            cleanup_tx();
        }
        if (active_rx) {
            cleanup_rx();
        }

        converter.end();

        active_tx = false;
        active_rx = false;
        active = false;
    }

    // Writes the data to the Digital to Analog Converter
    // ----------------------------------------------------------
    size_t write(const uint8_t *src, size_t size_bytes) override {
        // TRACED();
        // convert any format to int16_t
        return converter.write(src, size_bytes);
    }

    // Reads data from DMA buffer of the Analog to Digital Converter
    // ----------------------------------------------------------
    size_t readBytes(uint8_t *dest, size_t size_bytes) override {
        // TRACED();
        // Use the IO16Bit class for reading
        return io.readBytes(dest, size_bytes);
    }

    // How much data will there be available after reading ADC buffer
    // ----------------------------------------------------------
    int available() override {
        if (!active_rx) return 0;
        int buffered = availableFromFifoBytes();
        return buffered > 0 ? buffered : configuredRxBytes();
    }

protected:

    /**
     * @brief Custom FIFO class
     */
    template<typename T>
    class FIFO {
    public:
    
        FIFO() : size_(0), buffer_(nullptr), head_(0), tail_(0), count_(0) {}

        FIFO(size_t size) : size_(size), buffer_(new T[size]), head_(0), tail_(0), count_(0) {}

        ~FIFO() {
            delete[] buffer_;
            //LOGD("FIFO destroyed: size: %d, count: %d", size_, count_);
        }

        bool push(const T& value) {
            if (count_ < size_) {
                buffer_[tail_] = value;
                //LOGD("FIFO push - Value: %d at location: %d", value, tail_);
                tail_ = (tail_ + 1) % size_;
                count_++;
                //LOGD("FIFO push updated tail: %d, count: %d", tail_, count_);
                return true;
            }
            //LOGD("FIFO push failed - count %d > size  %d", value, count_, size_);
            return false; // Buffer full
        }

        bool pop(T& value) {
            if (count_ > 0) {
                value = buffer_[head_];
                //LOGD("FIFO pop - Value: %d at location: %d", value, head_);
                head_ = (head_ + 1) % size_;
                count_--;
                //LOGD("FIFO pop updated head: %d, count: %d", head_, count_);
                return true;
            }
            //LOGD("FIFO pop failed - count %d == 0", count_);
            return false; // Buffer empty
        }

        size_t size() const {
            return count_;
        }

        bool empty() const {
            return count_ == 0;
        }

        bool full() const {
            return count_ == size_;
        }

        void clear() {
            head_ = 0;
            tail_ = 0;
            count_ = 0;
        }

    private:
        size_t size_;
        T* buffer_;
        size_t head_;
        size_t tail_;
        size_t count_;
    };

    adc_continuous_handle_t adc_handle = nullptr;
    adc_cali_handle_t adc_cali_handle = nullptr;
    bool adc_cali_handle_active = false;
    AnalogConfigESP32V1 cfg;
    bool active = false;
    bool active_tx = false;
    bool active_rx = false;
    bool rx_started = false;
    int rx_pins_attached = 0;
    ConverterAutoCenter auto_center;
    #ifdef HAS_ESP32_DAC
    dac_continuous_handle_t dac_handle = nullptr;
    #endif
    
    // create array of FIFO buffers, one for each channel
    FIFO<ADC_DATA_TYPE>** fifo_buffers = nullptr;
    adc_digi_output_data_t* rx_result_buffer = nullptr;
    size_t rx_result_buffer_samples = 0;
    size_t rx_result_buffer_bytes = 0;

    int configuredRxBytes() const {
        return (cfg.buffer_size > 0) ? (int)(cfg.buffer_size * sizeof(int16_t)) : 0;
    }

    int availableFramesFromFifos() const {
        if (fifo_buffers == nullptr || cfg.channels <= 0) return 0;
        size_t min_samples = fifo_buffers[0]->size();
        for (int i = 1; i < cfg.channels; ++i) {
            size_t fifo_size = fifo_buffers[i]->size();
            if (fifo_size < min_samples) {
                min_samples = fifo_size;
            }
        }
        return (int)min_samples;
    }

    int availableFromFifoBytes() const {
        return availableFramesFromFifos() * cfg.channels * (int)sizeof(int16_t);
    }

    size_t fifoCapacityFromConvFrameBytes(size_t conv_frame_bytes) const {
        if (cfg.channels <= 0) return 8U;

        size_t fallback = 8U;
        if (SOC_ADC_DIGI_RESULT_BYTES == 0) return fallback;

        size_t frame_results = conv_frame_bytes / SOC_ADC_DIGI_RESULT_BYTES;
        if (frame_results == 0) return fallback;

        size_t frame_results_per_channel = frame_results / (size_t)cfg.channels;
        if (frame_results_per_channel == 0) return fallback;

        return frame_results_per_channel + 4U;
    }

    int getChannelIndex(ADC_CHANNEL_TYPE chan_num) const {
        for (int j = 0; j < cfg.channels; ++j) {
            if (cfg.adc_channels[j] == chan_num) {
                return j;
            }
        }
        return -1;
    }

    void resetReorderState() {
        if (fifo_buffers == nullptr) return;
        for (int i = 0; i < cfg.channels; ++i) {
            if (fifo_buffers[i] != nullptr) {
                fifo_buffers[i]->clear();
            }
        }
    }

    void cleanupScratchBuffer() {
        delete[] rx_result_buffer;
        rx_result_buffer = nullptr;
        rx_result_buffer_samples = 0;
        rx_result_buffer_bytes = 0;
    }

    bool isValidType2Record(const adc_digi_output_data_t& sample) const {
        if (cfg.adc_output_type != ADC_DIGI_OUTPUT_FORMAT_TYPE2) return true;

        uint32_t chan_num = sample.type2.channel;
        if (chan_num >= SOC_ADC_CHANNEL_NUM(cfg.adc_unit)) {
            LOGE("Invalid TYPE2 ADC channel: %u", (unsigned)chan_num);
            return false;
        }

#ifdef ADC_CONV_SINGLE_UNIT_1
        if (cfg.adc_conversion_mode == ADC_CONV_SINGLE_UNIT_1 &&
            sample.type2.unit != 0) {
            LOGE("Invalid TYPE2 ADC unit for ADC1 mode: %u",
                 (unsigned)sample.type2.unit);
            return false;
        }
#endif
#ifdef ADC_CONV_SINGLE_UNIT_2
        if (cfg.adc_conversion_mode == ADC_CONV_SINGLE_UNIT_2 &&
            sample.type2.unit != 1) {
            LOGE("Invalid TYPE2 ADC unit for ADC2 mode: %u",
                 (unsigned)sample.type2.unit);
            return false;
        }
#endif
        return true;
    }

    int16_t getOutputSample(ADC_DATA_TYPE data) {
        if (!cfg.adc_calibration_active) {
            return static_cast<int16_t>(data);
        }

        int data_milliVolts = 0;
        esp_err_t err = adc_cali_raw_to_voltage(adc_cali_handle, (int)data,
                                                &data_milliVolts);
        if (err != ESP_OK) {
            LOGE("adc_cali_raw_to_voltage error: %d", err);
            return 0;
        }
        return static_cast<int16_t>(data_milliVolts);
    }

    bool pushAdcChunkToReorderBuffers(const adc_digi_output_data_t* data,
                                      int samples_read) {
        if (data == nullptr) return false;

        for (int i = 0; i < samples_read; ++i) {
            const adc_digi_output_data_t* sample = &data[i];
            if (!isValidType2Record(*sample)) {
                LOGE("ADC reorder resync on invalid TYPE2 record at sample %d", i);
                resetReorderState();
                return false;
            }

            ADC_CHANNEL_TYPE chan_num = AUDIO_ADC_GET_CHANNEL(sample);
            int channel_idx = getChannelIndex(chan_num);
            if (channel_idx < 0) {
                LOGE("ADC reorder resync on invalid channel %u at sample %d",
                     (unsigned)chan_num, i);
                resetReorderState();
                return false;
            }

            ADC_DATA_TYPE sample_value = AUDIO_ADC_GET_DATA(sample);
            if (!fifo_buffers[channel_idx]->push(sample_value)) {
                LOGE("ADC reorder FIFO overflow on channel index %d (channel %u)",
                     channel_idx, (unsigned)chan_num);
                resetReorderState();
                return false;
            }
        }
        return true;
    }

    int emitFramesFromFifos(int16_t* dest, int max_frames) {
        if (dest == nullptr || max_frames <= 0 || cfg.channels <= 0) return 0;

        int frames_available = availableFramesFromFifos();
        int frames_to_emit =
            (frames_available < max_frames) ? frames_available : max_frames;
        int frames_emitted = 0;

        for (int frame = 0; frame < frames_to_emit; ++frame) {
            ADC_DATA_TYPE frame_data[NUM_ADC_CHANNELS];
            for (int ch = 0; ch < cfg.channels; ++ch) {
                if (!fifo_buffers[ch]->pop(frame_data[ch])) {
                    LOGE("ADC reorder pop failed on channel index %d", ch);
                    resetReorderState();
                    return frames_emitted;
                }
            }

            for (int ch = 0; ch < cfg.channels; ++ch) {
                dest[frames_emitted * cfg.channels + ch] =
                    getOutputSample(frame_data[ch]);
            }
            ++frames_emitted;
        }

        return frames_emitted;
    }

    void cleanupFifoBuffers() {
        if (fifo_buffers == nullptr) return;
        for (int i = 0; i < cfg.channels; ++i) {
            delete fifo_buffers[i];
        }
        delete[] fifo_buffers;
        fifo_buffers = nullptr;
    }

    void cleanupADCCalibration() {
        if (!adc_cali_handle_active || adc_cali_handle == nullptr) return;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
#elif !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4)
        adc_cali_delete_scheme_line_fitting(adc_cali_handle);
#endif
        adc_cali_handle = nullptr;
        adc_cali_handle_active = false;
    }

#ifdef ARDUINO
    void cleanupAttachedRxPins() {
        perimanSetBusDeinit(ESP32_BUS_TYPE_ADC_CONT, adcDetachBus);
        for (int i = 0; i < rx_pins_attached; ++i) {
            adc_channel_t adc_channel = cfg.adc_channels[i];
            int io_pin;
            adc_continuous_channel_to_io(cfg.adc_unit, adc_channel, &io_pin);
            if (perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_CONT) {
                if (!perimanClearPinBus(io_pin)) {
                    LOGE("perimanClearPinBus failed!");
                }
            }
        }
        rx_pins_attached = 0;
    }
#endif

    // 16Bit Audiostream for ESP32
    // ----------------------------------------------------------
    class IO16Bit : public AudioStream {
    public:
        IO16Bit(AnalogDriverESP32V1 *driver) { self = driver; }

        // Write int16_t data to the Digital to Analog Converter
        // ----------------------------------------------------------
        size_t write(const uint8_t *src, size_t size_bytes) override {
            // TRACED();
            #ifdef HAS_ESP32_DAC
            size_t result = 0;
            // Convert signed 16-bit to unsigned 8-bit
            int16_t *data16 = (int16_t *)src;
            uint8_t *data8 = (uint8_t *)src;
            int samples = size_bytes / 2;

            // Process data in batches to reduce the number of conversions and writes
            for (int j = 0; j < samples; j++) {
                data8[j] = (32768u + data16[j]) >> 8;
            }

            if (dac_continuous_write(self->dac_handle, data8, samples, &result, self->cfg.timeout) != ESP_OK) {
                result = 0;
            }
            return result * 2;
            #else
            return 0;
            #endif
        }

        // Read int16_t data from Analog to Digital Converter
        // ----------------------------------------------------------
        // FYI
        // typedef struct {
        //     union {
        //         struct {
        //             uint16_t data:     12;  /*!<ADC real output data info. Resolution: 12 bit. */
        //             uint16_t channel:   4;  /*!<ADC channel index info. */
        //         } type1;                    /*!<ADC type1 */
        //         struct {
        //             uint16_t data:     11;  /*!<ADC real output data info. Resolution: 11 bit. */
        //             uint16_t channel:   4;  /*!<ADC channel index info. For ESP32-S2:
        //                                         If (channel < ADC_CHANNEL_MAX), The data is valid.
        //                                         If (channel > ADC_CHANNEL_MAX), The data is invalid. */
        //             uint16_t unit:      1;  /*!<ADC unit index info. 0: ADC1; 1: ADC2.  */
        //         } type2;                    /*!<When the configured output format is 11bit.*/
        //         uint16_t val;               /*!<Raw data value */
        //     };
        // } adc_digi_output_data_t;     

        size_t readBytes(uint8_t *dest, size_t size_bytes) {
            if (dest == nullptr || size_bytes == 0 || self->cfg.channels <= 0) {
                return 0;
            }
            if (self->rx_result_buffer == nullptr || self->rx_result_buffer_bytes == 0) {
                LOGE("ADC RX scratch buffer is not initialized");
                return 0;
            }

            const size_t frame_size_bytes =
                (size_t)self->cfg.channels * sizeof(int16_t);
            const size_t frames_requested = size_bytes / frame_size_bytes;
            if (frames_requested == 0) {
                return 0;
            }

            int16_t* result16 = reinterpret_cast<int16_t*>(dest);
            int frames_provided =
                self->emitFramesFromFifos(result16, (int)frames_requested);

            while (frames_provided < (int)frames_requested) {
                uint32_t bytes_read = 0;
                esp_err_t err = adc_continuous_read(
                    self->adc_handle,
                    reinterpret_cast<uint8_t*>(self->rx_result_buffer),
                    (uint32_t)self->rx_result_buffer_bytes, &bytes_read,
                    (uint32_t)self->cfg.timeout);
                if (err != ESP_OK) {
                    if (err != ESP_ERR_TIMEOUT) {
                        LOGE("adc_continuous_read unsuccessful: %d", err);
                    }
                    break;
                }

                int samples_read = bytes_read / sizeof(adc_digi_output_data_t);
                if (samples_read <= 0) {
                    break;
                }

                if (!self->pushAdcChunkToReorderBuffers(self->rx_result_buffer,
                                                        samples_read)) {
                    break;
                }

                frames_provided += self->emitFramesFromFifos(
                    result16 + (frames_provided * self->cfg.channels),
                    (int)frames_requested - frames_provided);
            }

            size_t bytes_provided = (size_t)frames_provided * frame_size_bytes;
            if (bytes_provided > 0 && self->cfg.is_auto_center_read) {
                self->auto_center.convert(dest, bytes_provided);
            }
            return bytes_provided;
        }

    protected:
        AnalogDriverESP32V1 *self = nullptr;

    } io{this};

    NumberFormatConverterStream converter{io};

    // Setup Digital to Analog
    // ----------------------------------------------------------
    #ifdef HAS_ESP32_DAC
    bool setup_tx() {
        dac_continuous_config_t cont_cfg = {
            .chan_mask = cfg.channels == 1 ? cfg.dac_mono_channel : DAC_CHANNEL_MASK_ALL,
            .desc_num = (uint32_t)cfg.buffer_count,
            .buf_size = (size_t)cfg.buffer_size,
            .freq_hz = (uint32_t)cfg.sample_rate,
            .offset = 0,
            .clk_src = cfg.use_apll ? DAC_DIGI_CLK_SRC_APLL : DAC_DIGI_CLK_SRC_DEFAULT, // Using APLL as clock source to get a wider frequency range
            .chan_mode = cfg.channels == 1 ? DAC_CHANNEL_MODE_SIMUL : DAC_CHANNEL_MODE_ALTER,
        };
        // Allocate continuous channels
        if (dac_continuous_new_channels(&cont_cfg, &dac_handle) != ESP_OK) {
            LOGE("new_channels");
            return false;
        }
        if (dac_continuous_enable(dac_handle) != ESP_OK) {
            LOGE("enable");
            return false;
        }
        return true;
    }
    #else
    bool setup_tx() {
        LOGE("DAC not supported");
        return false;
    }
    #endif

    // Setup Analog to Digital Converter
    // ----------------------------------------------------------
    bool setup_rx() {
        adc_channel_t adc_channel;
        int io_pin;
        esp_err_t err; 

        // Check the configuration
        if (!checkADCChannels())      return false;
        if (!checkADCSampleRate())    return false;
        if (!checkADCBitWidth())      return false;
        if (!checkADCBitsPerSample()) return false;

        if (adc_handle != nullptr) {
            LOGE("adc unit %u continuous is already initialized. Please call end() first!", cfg.adc_unit);
            return false;
        }

        #ifdef ARDUINO
        // Set periman deinit callback
        // TODO, currently handled in end() method

        // Set the pins/channels to INIT state
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel = cfg.adc_channels[i];
            adc_continuous_channel_to_io(cfg.adc_unit, adc_channel, &io_pin);
            if (!perimanClearPinBus(io_pin)) {
                LOGE("perimanClearPinBus failed!");
                return false;
            }
        }
        #endif

        uint32_t conv_frame_size = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
        #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        conv_frame_size = adcContinuousAlignFrameSize(conv_frame_size);
        #endif

        uint32_t rx_max_conv_frame_bytes = adcContinuousMaxConvFrameBytes();
        uint32_t max_samples_per_frame = adcContinuousMaxResultsPerFrame();
        if (conv_frame_size > rx_max_conv_frame_bytes) {
            LOGE(
                "buffer_size is too big for one ADC DMA frame: %u samples = %u "
                "bytes, max %u samples / %u bytes",
                cfg.buffer_size, (unsigned)conv_frame_size,
                (unsigned)max_samples_per_frame,
                (unsigned)rx_max_conv_frame_bytes);
            return false;
        } else {
            LOGI(
                "RX DMA frame: %u conversion results, %u bytes (max %u results / "
                "%u bytes)",
                cfg.buffer_size, (unsigned)conv_frame_size,
                (unsigned)max_samples_per_frame,
                (unsigned)rx_max_conv_frame_bytes);
        }
        
        // Create adc_continuous handle
        adc_continuous_handle_cfg_t adc_config;
        uint32_t rx_frame_count = cfg.buffer_count > 0 ? (uint32_t)cfg.buffer_count : 1U;
        adc_config.max_store_buf_size = (uint32_t)conv_frame_size * rx_frame_count;
        adc_config.conv_frame_size = (uint32_t) conv_frame_size;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        adc_config.flags.flush_pool  = true;
#endif        
        LOGI("RX pool: %u frames, %u bytes", (unsigned)rx_frame_count,
             (unsigned)adc_config.max_store_buf_size);
        err = adc_continuous_new_handle(&adc_config, &adc_handle);
        if (err != ESP_OK) {
            LOGE("adc_continuous_new_handle failed with error: %d", err);
            return false;
        } else {
            LOGI("adc_continuous_new_handle successful");
        }

        // Configure the ADC patterns
        adc_digi_pattern_config_t adc_pattern[cfg.channels] = {};
        for (int i = 0; i < cfg.channels; i++) {
            uint8_t ch = cfg.adc_channels[i];
            adc_pattern[i].atten = (uint8_t) cfg.adc_attenuation;
            adc_pattern[i].channel = (uint8_t)ch;
            adc_pattern[i].unit = (uint8_t) cfg.adc_unit;
            adc_pattern[i].bit_width = (uint8_t) cfg.adc_bit_width;
        }

        // Configure the ADC
        adc_continuous_config_t dig_cfg = {
            .pattern_num = (uint32_t) cfg.channels,
            .adc_pattern = adc_pattern,
            .sample_freq_hz = (uint32_t)cfg.sample_rate * cfg.channels,
            .conv_mode = (adc_digi_convert_mode_t) cfg.adc_conversion_mode,
            .format = (adc_digi_output_format_t) cfg.adc_output_type,
        };

        // Log the configuration
        LOGI("dig_cfg.sample_freq_hz: %u", (unsigned)dig_cfg.sample_freq_hz);
        LOGI("dig_cfg.conv_mode: %u (1: unit 1, 2: unit 2, 3: both)", dig_cfg.conv_mode);
        LOGI("dig_cfg.format: %u (0 is type1: [12bit data, 4bit channel])", dig_cfg.format);
        for (int i = 0; i < cfg.channels; i++) {
            LOGI("dig_cfg.adc_pattern[%d].atten: %u", i, dig_cfg.adc_pattern[i].atten);
            LOGI("dig_cfg.adc_pattern[%d].channel: %u", i, dig_cfg.adc_pattern[i].channel);
            LOGI("dig_cfg.adc_pattern[%d].unit: %u", i, dig_cfg.adc_pattern[i].unit);
            LOGI("dig_cfg.adc_pattern[%d].bit_width: %u", i, dig_cfg.adc_pattern[i].bit_width);
        }

        // Initialize ADC
        err = adc_continuous_config(adc_handle, &dig_cfg);
        if (err != ESP_OK) {
            LOGE("adc_continuous_config unsuccessful with error: %d", err);
            cleanup_rx();
            return false;
        }
        LOGI("adc_continuous_config successful");

        // Set up optional calibration
        if (!setupADCCalibration()) {
            cleanup_rx();
            return false;
        }

        // Attach the pins to the ADC unit
#ifdef ARDUINO
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel = cfg.adc_channels[i];
            adc_continuous_channel_to_io(cfg.adc_unit, adc_channel, &io_pin);
            // perimanSetPinBus: uint8_t pin, peripheral_bus_type_t type, void * bus, int8_t bus_num, int8_t bus_channel
            if (!perimanSetPinBus(io_pin, ESP32_BUS_TYPE_ADC_CONT, (void *)(cfg.adc_unit + 1), cfg.adc_unit, adc_channel)) {
                LOGE("perimanSetPinBus to Continuous an ADC Unit %u failed!", cfg.adc_unit);
                cleanup_rx();
                return false;
            }
            rx_pins_attached = i + 1;
        }
#endif

        // Start ADC
        err = adc_continuous_start(adc_handle);
        if (err != ESP_OK) {
            LOGE("adc_continuous_start unsuccessful with error: %d", err);
            cleanup_rx();
            return false;
        }
        rx_started = true;

        // Setup up optimal auto center which puts the avg at 0
        auto_center.begin(cfg.channels, cfg.bits_per_sample, true);

        cleanupScratchBuffer();
        rx_result_buffer_bytes = conv_frame_size;
        rx_result_buffer_samples =
            rx_result_buffer_bytes / sizeof(adc_digi_output_data_t);
        rx_result_buffer = new adc_digi_output_data_t[rx_result_buffer_samples];
        if (rx_result_buffer == nullptr || rx_result_buffer_samples == 0) {
            LOGE("Failed to allocate ADC RX scratch buffer");
            cleanup_rx();
            return false;
        }
        LOGI("ADC RX scratch buffer allocated for %u bytes / %u samples",
             (unsigned)rx_result_buffer_bytes, (unsigned)rx_result_buffer_samples);

        // Keep the reorder FIFOs small: they absorb channel skew only and are
        // not sized for the caller's full readBytes() window.
        size_t fifo_size = fifoCapacityFromConvFrameBytes(conv_frame_size);
        fifo_buffers = new FIFO<ADC_DATA_TYPE>*[cfg.channels](); // Allocate an array of FIFO objects
        for (int i = 0; i < cfg.channels; ++i) {
            fifo_buffers[i] = new FIFO<ADC_DATA_TYPE>(fifo_size);
        }
        resetReorderState();
        LOGI("%d FIFO buffers allocated of size %u from DMA frame %u bytes",
             cfg.channels, (unsigned)fifo_size, (unsigned)conv_frame_size);
        
        LOGI("Setup ADC successful");

        return true;
    }

    /// Cleanup dac
    bool cleanup_tx() {
        bool ok = true;
#ifdef HAS_ESP32_DAC
        if (dac_handle==nullptr) return true;
        if (dac_continuous_disable(dac_handle) != ESP_OK){
            ok = false;
            LOGE("dac_continuous_disable failed");
        }
        if (dac_continuous_del_channels(dac_handle) != ESP_OK){
            ok = false;
            LOGE("dac_continuous_del_channels failed");
        }
        dac_handle = nullptr;
#endif
        return ok;
    }

#ifdef ARDUINO
    // dummy detach: w/o this it's failing
    static bool adcDetachBus(void *bus) {
        LOGD("===> adcDetachBus: %d", (int) bus);
        return true;
    }
#endif

    /// Cleanup Analog to Digital Converter
    bool cleanup_rx() {
        bool ok = true;
        if (adc_handle != nullptr && rx_started) {
            if (adc_continuous_stop(adc_handle) != ESP_OK) {
                LOGE("adc_continuous_stop failed");
                ok = false;
            }
            rx_started = false;
        }
        if (adc_handle != nullptr) {
            if (adc_continuous_deinit(adc_handle) != ESP_OK) {
                LOGE("adc_continuous_deinit failed");
                ok = false;
            }
            adc_handle = nullptr;
        }

        cleanupADCCalibration();
        cleanupFifoBuffers();
        cleanupScratchBuffer();

#ifdef ARDUINO
        cleanupAttachedRxPins();
#endif
        return ok;
    }

    /// ----------------------------------------------------------
    bool checkADCBitWidth() {
        if ((cfg.adc_bit_width < SOC_ADC_DIGI_MIN_BITWIDTH) ||
            (cfg.adc_bit_width > SOC_ADC_DIGI_MAX_BITWIDTH)) {
            LOGE("adc bit width: %u cannot be set, range: %u to %u", cfg.adc_bit_width,
                (unsigned)SOC_ADC_DIGI_MIN_BITWIDTH, (unsigned)SOC_ADC_DIGI_MAX_BITWIDTH);
            return false;
        }
        LOGI("adc bit width: %u, range: %u to %u", cfg.adc_bit_width,
                (unsigned)SOC_ADC_DIGI_MIN_BITWIDTH, (unsigned)SOC_ADC_DIGI_MAX_BITWIDTH);
        return true;
    }

    /// ----------------------------------------------------------
    bool checkADCChannels() {
        int io_pin;
        adc_channel_t adc_channel;

        int max_channels = sizeof(cfg.adc_channels) / sizeof(adc_channel_t);
        if (cfg.channels > max_channels) {
            LOGE("number of channels: %d, max: %d", cfg.channels, max_channels);
            return false;
        }
        LOGI("channels: %d, max: %d", cfg.channels, max_channels);

        // Lets make sure the adc channels are available
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel = cfg.adc_channels[i];
            auto err = adc_continuous_channel_to_io(cfg.adc_unit, adc_channel, &io_pin);
            if (err != ESP_OK) {
                LOGE("ADC channel %u is not available on ADC unit %u", adc_channel, cfg.adc_unit);
                return false;
            } else {
                LOGI("ADC channel %u is on pin %u", adc_channel, io_pin);
            }
        }
        return true;
    }

    /// ----------------------------------------------------------
    bool checkADCSampleRate() {
        int sample_rate = cfg.sample_rate * cfg.channels;
        if ((sample_rate < SOC_ADC_SAMPLE_FREQ_THRES_LOW) ||
            (sample_rate > SOC_ADC_SAMPLE_FREQ_THRES_HIGH)) {
            LOGE("sample rate eff: %u can not be set, range: %u to %u", sample_rate,
                SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);
            return false;
        }
        LOGI("sample rate eff: %u, range: %u to %u", sample_rate,
                SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH);

        return true;
    }

    /// ----------------------------------------------------------
    bool checkADCBitsPerSample() {
        int supported_bits = 16; // for the time being we support only 16 bits!

        // calculated default value if nothing is specified
        if (cfg.bits_per_sample == 0) {
            cfg.bits_per_sample = supported_bits;
            LOGI("bits per sample set to: %d", cfg.bits_per_sample);
        }

        // check bits_per_sample
        if (cfg.bits_per_sample != supported_bits) {
            LOGE("bits per sample:  error. It should be: %d but is %d",
                supported_bits, cfg.bits_per_sample);
            return false;
        }
        LOGI("bits per sample: %d", cfg.bits_per_sample);
        return true;
    }

    /// ----------------------------------------------------------
    bool setupADCCalibration() {
        if (!cfg.adc_calibration_active)
            return true;

        // Initialize ADC calibration handle
        // Calibration is applied to an ADC unit (not per channel). 

        // setup calibration only when requested
        esp_err_t err = ESP_OK;

        if (adc_cali_handle_active && adc_cali_handle != nullptr) {
            return true;
        }

        if (adc_cali_handle == NULL) {
            #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            // curve fitting is preferred
            adc_cali_curve_fitting_config_t cali_config;
            cali_config.unit_id = cfg.adc_unit;
            cali_config.atten = (adc_atten_t)cfg.adc_attenuation;
            cali_config.bitwidth = (adc_bitwidth_t)cfg.adc_bit_width;
            err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
            #elif !defined(CONFIG_IDF_TARGET_ESP32H2) && !defined(CONFIG_IDF_TARGET_ESP32P4)
            // line fitting is the alternative
            adc_cali_line_fitting_config_t cali_config;
            cali_config.unit_id = cfg.adc_unit;
            cali_config.atten = (adc_atten_t)cfg.adc_attenuation;
            cali_config.bitwidth = (adc_bitwidth_t)cfg.adc_bit_width;
            err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
            #endif
            if (err != ESP_OK) {
                adc_cali_handle = nullptr;
                adc_cali_handle_active = false;
                LOGE("creating calibration handle failed for ADC%d with atten %d and bitwidth %d",
                    cfg.adc_unit, cfg.adc_attenuation, cfg.adc_bit_width);
                return false;
            } else {
                adc_cali_handle_active = true;
                LOGI("enabled calibration for ADC%d with atten %d and bitwidth %d",
                    cfg.adc_unit, cfg.adc_attenuation, cfg.adc_bit_width);
            }
        }
        return true;
    }

};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

} // namespace audio_tools

#endif
