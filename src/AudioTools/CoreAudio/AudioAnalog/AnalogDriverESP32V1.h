#pragma once

#include "AudioConfig.h"

#if defined(ESP32) && defined(USE_ANALOG) && \
    ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) || defined(DOXYGEN)

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
                LOGE("mode");
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
        return active_rx ? (uint32_t)(cfg.buffer_size * sizeof(int16_t)) : 0;
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
    AnalogConfigESP32V1 cfg;
    bool active = false;
    bool active_tx = false;
    bool active_rx = false;
    ConverterAutoCenter auto_center;
    #ifdef HAS_ESP32_DAC
    dac_continuous_handle_t dac_handle = nullptr;
    #endif
    
    // create array of FIFO buffers, one for each channel
    FIFO<ADC_DATA_TYPE>** fifo_buffers;

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
            // TRACED();

            size_t total_bytes = 0;
            size_t bytes_provided = 0;
            int min_samples_in_fifo_per_channel = 0;
            int max_samples_in_fifo_per_channel = 0;
            int samples_read  =0;
            int fifo_size = 0;
            int idx = -1;
            int samples_provided_per_channel = 0;
            int data_milliVolts = 0;

            int samples_requested = size_bytes / sizeof(int16_t);
            int samples_requested_per_channel = samples_requested/self->cfg.channels;
            // for the adc_continuous_read function
            adc_digi_output_data_t* result_data = (adc_digi_output_data_t*)malloc(samples_requested * sizeof(adc_digi_output_data_t));
            if (result_data == NULL) {
                LOGE("Failed to allocate memory for result_data");
                return 0; // Handle memory allocation failure
            }
            memset(result_data, 0, samples_requested * sizeof(adc_digi_output_data_t));
            uint32_t bytes_read; // bytes from ADC buffer read

            // for output buffer
            uint16_t *result16 = (uint16_t *)dest; // pointer to the destination buffer
            uint16_t *end = (uint16_t *)(dest + size_bytes); // pointer to the end of the destination buffer

            // 1) read the requested bytes from the buffer
            // LOGI("adc_continuous_read request:%d samples %d bytes requested", samples_requested, (uint32_t)(samples_requested * sizeof(adc_digi_output_data_t)));
            if (adc_continuous_read(self->adc_handle, (uint8_t *)result_data, (uint32_t)(samples_requested * sizeof(adc_digi_output_data_t)), &bytes_read, (uint32_t)self->cfg.timeout) == ESP_OK) {
                samples_read = bytes_read / sizeof(adc_digi_output_data_t);
                LOGD("adc_continuous_read -> %u bytes / %d samples of %d bytes requested", (unsigned)bytes_read, samples_read, (int)(samples_requested * sizeof(adc_digi_output_data_t)));

                // Parse and store data in FIFO buffers
                for (int i = 0; i < samples_read; i++) {
                    adc_digi_output_data_t *p = &result_data[i];
                    ADC_CHANNEL_TYPE chan_num = AUDIO_ADC_GET_CHANNEL(p);
                    ADC_DATA_TYPE data = AUDIO_ADC_GET_DATA(p);

                    // Find the index of the channel in the configured channels
                    idx = -1;
                    for (int j = 0; j < self->cfg.channels; ++j) {
                        if (self->cfg.adc_channels[j] == chan_num) {
                            idx = j;
                            break;
                        }
                    }
                    // Push the data to the corresponding FIFO buffer
                    if (idx >= 0) {
                        if (self->fifo_buffers[idx]->push(data)) {
                            LOGD("Sample %d, FIFO %d, ch %u, d %u", i, idx, chan_num, data);
                        } else {
                            LOGE("Sample %d, FIFO buffer is full, ch %u, d %u", i, (unsigned)chan_num, data);
                        }
                    } else {
                        LOGE("Sample %d, ch %u not found in configuration, d: %u", i, (unsigned)chan_num, data);
                        for (int k = 0; k < self->cfg.channels; ++k) {
                            LOGE("Available config ch: %u", self->cfg.adc_channels[k]);
                        }
                    }

                }

                // Determine min number of samples from all FIFO buffers
                min_samples_in_fifo_per_channel = self->fifo_buffers[0]->size();
                for (int i = 1; i < self->cfg.channels; i++) {
                    fifo_size = self->fifo_buffers[i]->size();
                    if (fifo_size < min_samples_in_fifo_per_channel) {
                        min_samples_in_fifo_per_channel = fifo_size;
                    }
                }

                // 2) If necessary, top off the FIFO buffers to return the requested number of bytes
                while (samples_requested_per_channel > min_samples_in_fifo_per_channel) {

                    // obtain two extra sets of data (2 because number of bytes requested from ADC buffer needs to be divisible by 4)
                    // LOGI("adc_continuous_read request:%d samples %d bytes requested", samples_requested, (uint32_t)(samples_requested * sizeof(adc_digi_output_data_t)));
                    if (adc_continuous_read(self->adc_handle, (uint8_t *)result_data, (uint32_t)(2*self->cfg.channels * sizeof(adc_digi_output_data_t)), &bytes_read, (uint32_t)self->cfg.timeout) != ESP_OK) {
                        LOGE("Top off, adc_continuous_read unsuccessful");
                        break;
                    }

                    // Parse the additional data
                    samples_read = bytes_read / sizeof(adc_digi_output_data_t);
                    LOGD("Top Off: Requested %d samples per Channel, Min samples in FIFO: %d, Read additional %d bytes / %d samples", samples_requested_per_channel, min_samples_in_fifo_per_channel, (unsigned)bytes_read, samples_read);

                    for (int i = 0; i < samples_read; i++) {
                        adc_digi_output_data_t *p = &result_data[i];
                        ADC_CHANNEL_TYPE chan_num = AUDIO_ADC_GET_CHANNEL(p);
                        ADC_DATA_TYPE data = AUDIO_ADC_GET_DATA(p);

                        // Find the index of the channel in the configured channels
                        idx = -1;
                        for (int j = 0; j < self->cfg.channels; ++j) {
                            if (self->cfg.adc_channels[j] == chan_num) {
                                idx = j;
                                break;
                            }
                        }
                        // Push the data to the corresponding FIFO buffer
                        if (idx >= 0) {
                            if (self->fifo_buffers[idx]->push(data)) {
                                LOGD("Top Off Sample %d, FIFO %d, ch %u, d %u", i, idx, chan_num, data);
                            } else {
                                LOGE("Top Off Sample %d, FIFO buffer is full, ch %u, d %u", i,  chan_num, data);
                            }
                        } else {
                            LOGE("Top Off Sample %d, ch %u not found in configuration, d %u", i, chan_num, data);
                            for (int k = 0; k < self->cfg.channels; ++k) {
                                LOGE("Available config ch: %u", self->cfg.adc_channels[k]);
                            }
                        }
                    }

                    // Determine the updated minimal number of samples in FIFO buffers
                    min_samples_in_fifo_per_channel = self->fifo_buffers[0]->size();
                    max_samples_in_fifo_per_channel = self->fifo_buffers[0]->size();
                    for (int i = 1; i < self->cfg.channels; i++) {
                        fifo_size = self->fifo_buffers[i]->size();
                        if (fifo_size < min_samples_in_fifo_per_channel) {
                            min_samples_in_fifo_per_channel = fifo_size;
                        }
                        if (fifo_size > max_samples_in_fifo_per_channel) {
                            max_samples_in_fifo_per_channel = fifo_size;
                        }
                    }
                    LOGD("Min # of samples in FIFO: %d, Max # of samples in FIFO: %d", min_samples_in_fifo_per_channel, max_samples_in_fifo_per_channel);
                }

                // 3) Calibrate and copy data to the output buffer
                if (samples_requested_per_channel <= min_samples_in_fifo_per_channel) {
                    LOGD("Going to copying %d samples of %d samples/channel to output buffer", samples_requested, samples_requested_per_channel);
                    samples_provided_per_channel = samples_requested_per_channel;
                } else {
                    // This should  not happen as we topped off the FIFO buffers in step 2)
                    LOGE("Only %d samples per channel available for output buffer", min_samples_in_fifo_per_channel);
                    samples_provided_per_channel = min_samples_in_fifo_per_channel;
                }
                
                for (int i = 0; i < samples_provided_per_channel; i++) {
                    for (int j = 0; j < self->cfg.channels; j++) {
                        ADC_DATA_TYPE data;
                        self->fifo_buffers[j]->pop(data);
                        if (result16 < end) {
                            if (self->cfg.adc_calibration_active) {
                                // Provide result in millivolts
                                auto err = adc_cali_raw_to_voltage(self->adc_cali_handle, (int)data, &data_milliVolts);
                                if (err == ESP_OK) {
                                    *result16 = static_cast<int16_t>(data_milliVolts);
                                } else {
                                    LOGE("adc_cali_raw_to_voltage error: %d", err);
                                    *result16 = 0;
                                }
                            } else {
                                *result16 = data;
                            }
                            result16++;
                        } else {
                            LOGE("Buffer write overflow, skipping data");
                        }
                    }
                }

                bytes_provided = samples_provided_per_channel * self->cfg.channels * sizeof(int16_t);

                // 4) Engage centering if enabled
                if (self->cfg.is_auto_center_read) {
                    self->auto_center.convert(dest, bytes_provided);
                }

            } else {
                LOGE("adc_continuous_read unsuccessful");
                bytes_provided = 0;
            }
            free(result_data);
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
            .chan_mode = DAC_CHANNEL_MODE_ALTER,
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
            LOGE("adc unit %u continuous is already initialized. Please call end() first!", ADC_UNIT);
            return false;
        }

        #ifdef ARDUINO
        // Set periman deinit callback
        // TODO, currently handled in end() method

        // Set the pins/channels to INIT state
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel = cfg.adc_channels[i];
            adc_continuous_channel_to_io(ADC_UNIT, adc_channel, &io_pin);
            if (!perimanClearPinBus(io_pin)) {
                LOGE("perimanClearPinBus failed!");
                return false;
            }
        }
        #endif

        // Determine conv_frame_size which must be multiple of SOC_ADC_DIGI_DATA_BYTES_PER_CONV
        // Old Code
        uint32_t conv_frame_size = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
        #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        uint8_t calc_multiple = conv_frame_size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV;
        if (calc_multiple != 0) {
            conv_frame_size = (uint32_t) (conv_frame_size + calc_multiple);
        }
        #endif

        // Proposed new Code
        // uint32_t conv_frame_size = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_RESULT_BYTES;
        // #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        //     uint32_t conv_frame_size = (uint32_t)cfg.buffer_size * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
        // #endif

        // Conversion frame size buffer can't be bigger than 4092 bytes
        if (conv_frame_size > 4092) {
            LOGE("buffer_size is too big. Please set lower buffer_size.");
            return false;
        } else {
            LOGI("buffer_size: %u samples, conv_frame_size: %u bytes", cfg.buffer_size, (unsigned)conv_frame_size);
        }
        
        // Create adc_continuous handle
        adc_continuous_handle_cfg_t adc_config;
        adc_config.max_store_buf_size = (uint32_t)conv_frame_size * 2;
        adc_config.conv_frame_size = (uint32_t) conv_frame_size;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        adc_config.flags.flush_pool  = true;
#endif        
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
            adc_pattern[i].unit = (uint8_t) ADC_UNIT;
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
            return false;
        }
        LOGI("adc_continuous_config successful");

        // Set up optional calibration
        if (!setupADCCalibration()) {
            return false;
        }

        // Attach the pins to the ADC unit
        #ifdef ARDUINO
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel = cfg.adc_channels[i];
            adc_continuous_channel_to_io(ADC_UNIT, adc_channel, &io_pin);
            // perimanSetPinBus: uint8_t pin, peripheral_bus_type_t type, void * bus, int8_t bus_num, int8_t bus_channel
            if (!perimanSetPinBus(io_pin, ESP32_BUS_TYPE_ADC_CONT, (void *)(ADC_UNIT + 1), ADC_UNIT, adc_channel)) {
                LOGE("perimanSetPinBus to Continuous an ADC Unit %u failed!", ADC_UNIT);
                return false;
            }
        }
        #endif

        // Start ADC
        err = adc_continuous_start(adc_handle);
        if (err != ESP_OK) {
            LOGE("adc_continuous_start unsuccessful with error: %d", err);
            return false;
        }

        // Setup up optimal auto center which puts the avg at 0
        auto_center.begin(cfg.channels, cfg.bits_per_sample, true);

        // Initialize the FIFO buffers    
        size_t fifo_size = (cfg.buffer_size / cfg.channels) + 8; // Add a few extra elements
        fifo_buffers = new FIFO<ADC_DATA_TYPE>*[cfg.channels]; // Allocate an array of FIFO objects
        for (int i = 0; i < cfg.channels; ++i) {
            fifo_buffers[i] = new FIFO<ADC_DATA_TYPE>(fifo_size);
        }
        LOGI("%d FIFO buffers allocated of size %d", cfg.channels, fifo_size);
        
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
        if (adc_handle==nullptr) return true;
        adc_continuous_stop(adc_handle);
        adc_continuous_deinit(adc_handle);
        if (cfg.adc_calibration_active) {
            #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_delete_scheme_curve_fitting(adc_cali_handle);
            #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
            adc_cali_delete_scheme_line_fitting(adc_cali_handle);
            #endif
        }

        // Clean up the FIFO buffers
        if (fifo_buffers != nullptr) {
            for (int i = 0; i < cfg.channels; ++i) {
                delete fifo_buffers[i];
            }
            delete[] fifo_buffers;
            fifo_buffers = nullptr;
        }

#ifdef ARDUINO
        // Set all used pins/channels to INIT state
        perimanSetBusDeinit(ESP32_BUS_TYPE_ADC_CONT, adcDetachBus);
        for (int i = 0; i < cfg.channels; i++) {
            adc_channel_t adc_channel = cfg.adc_channels[i];
            int io_pin;
            adc_continuous_channel_to_io(ADC_UNIT, adc_channel, &io_pin);
            if (perimanGetPinBusType(io_pin) == ESP32_BUS_TYPE_ADC_CONT) {
                if (!perimanClearPinBus(io_pin)) {
                    LOGE("perimanClearPinBus failed!");
                }
            }
        }
#endif
        adc_handle = nullptr;
        return true; // Return true to indicate successful cleanup
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
            auto err = adc_continuous_channel_to_io(ADC_UNIT, adc_channel, &io_pin);
            if (err != ESP_OK) {
                LOGE("ADC channel %u is not available on ADC unit %u", adc_channel, ADC_UNIT);
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
        if (adc_cali_handle == NULL) {
            #if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            // curve fitting is preferred
            adc_cali_curve_fitting_config_t cali_config;
            cali_config.unit_id = ADC_UNIT;
            cali_config.atten = (adc_atten_t)cfg.adc_attenuation;
            cali_config.bitwidth = (adc_bitwidth_t)cfg.adc_bit_width;
            auto err = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
            #elif !defined(CONFIG_IDF_TARGET_ESP32H2)
            // line fitting is the alternative
            adc_cali_line_fitting_config_t cali_config;
            cali_config.unit_id = ADC_UNIT;
            cali_config.atten = (adc_atten_t)cfg.adc_attenuation;
            cali_config.bitwidth = (adc_bitwidth_t)cfg.adc_bit_width;
            auto err = adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle);
            #endif
            if (err != ESP_OK) {
                LOGE("creating calibration handle failed for ADC%d with atten %d and bitwidth %d",
                    ADC_UNIT, cfg.adc_attenuation, cfg.adc_bit_width);
                return false;
            } else {
                LOGI("enabled calibration for ADC%d with atten %d and bitwidth %d",
                    ADC_UNIT, cfg.adc_attenuation, cfg.adc_bit_width);
            }
        }
        return true;
    }

};

/// @brief AnalogAudioStream
using AnalogDriver = AnalogDriverESP32V1;

} // namespace audio_tools

#endif
