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
    AnalogDriverESP32V1() = default;

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
        TRACED();
        // Use the IO16Bit class for reading
        return io.readBytes(dest, size_bytes);
    }

    // How much data will there be available after reading ADC buffer
    // ----------------------------------------------------------
    int available() override {
        return active_rx ? (uint32_t)(cfg.buffer_size * sizeof(int16_t)) : 0;
    }

protected:

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
    

    // 16Bit Audiostream for ESP32
    // ----------------------------------------------------------
    class IO16Bit : public AudioStream {
    public:
        IO16Bit(AnalogDriverESP32V1 *driver) { self = driver; }

        /// Write int16_t data to the Digital to Analog Converter
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

        /// Read int16_t data from ADC
        size_t readBytes(uint8_t *dest, size_t size_bytes) {
            LOGD("readBytes: %d", (int) size_bytes);
            size_t result = 0;
            int channels = self->cfg.channels;
            assert(channels > 0);
            sampleIndex.resize(channels);
            for(int ch=0;ch < channels;ch++){
                sampleIndex[ch] = 0;
            }
            uint16_t *result16 = (uint16_t *)dest; // pointer to the destination buffer
            int samples_requested = size_bytes / sizeof(int16_t);
            int samples_requested_per_channel = samples_requested / channels;
            int buffer_size = samples_requested * sizeof(adc_digi_output_data_t);
            if (result_data.size() < buffer_size){
                result_data.resize(buffer_size);
            }

            int read_size_bytes = buffer_size;

            // read the requested bytes into the buffer
            while (read_size_bytes > 0){
                uint32_t bytes_read = 0;
                if (adc_continuous_read(self->adc_handle, (uint8_t *)result_data.data(), (uint32_t)read_size_bytes, &bytes_read, (uint32_t)self->cfg.timeout) == ESP_OK) {
                    int samples_read = bytes_read / sizeof(adc_digi_output_data_t);
                    LOGD("adc_continuous_read -> %u bytes / %d samples of %d bytes requested", (unsigned)bytes_read, samples_read, (int)(samples_requested * sizeof(adc_digi_output_data_t)));

                    // Provide samples to dest
                    for (int i = 0; i < samples_read; i++) {
                        adc_digi_output_data_t *p = (adc_digi_output_data_t*) &result_data[i];
                        ADC_CHANNEL_TYPE chan_num = AUDIO_ADC_GET_CHANNEL(p);
                        ADC_DATA_TYPE sample_value = AUDIO_ADC_GET_DATA(p);
                        int channel = getChannelIdx(chan_num);

                        // provide data to dest
                        if (channel >=0 && channel < channels){
                            int idx = channel + (sampleIndex[channel] * channels);
                            LOGI("idx for %d: %d", channel, idx);
                            if(idx < samples_requested){
                                // Provide result in millivolts
                                if (self->cfg.adc_calibration_active) {
                                    result16[idx] = getCalibratedValue(sample_value);
                                } else {
                                    result16[idx] = sample_value;
                                }
                                // increase index for the actual channel
                                sampleIndex[channel]++;
                            } else {
                                LOGE("Invalid idx: %d / max %d", idx, samples_requested);
                            }
                        } else {
                            LOGE("Invalid channel: %d", channel);
                        }
                    }
                    /// find channel with min samples
                    int samples_available = getMinSamplesForAllChannels(channels);
                    result = samples_available * channels * sizeof(int16_t);

                    /// Read some more samples if we do not have the requested size for all channels
                    read_size_bytes = result - size_bytes;
                    if (read_size_bytes > 0)
                        LOGI("read missing samples: %d", samples_requested_per_channel - samples_available);
                } else {
                    LOGE("adc_continuous_read error");
                    break;
                }
            }


            // Centering if enabled
            if (self->cfg.is_auto_center_read) {
                self->auto_center.convert(dest, result);
            }

            return result;
        }

    protected:
        AnalogDriverESP32V1 *self = nullptr;
        Vector<uint8_t> result_data{0};
        Vector<int> sampleIndex{0};

        int getChannelIdx(ADC_CHANNEL_TYPE chan_num){
            for (int j = 0; j < self->cfg.channels; ++j) {
                if (self->cfg.adc_channels[j] == chan_num) {
                    return j;
                }
            }
            return -1;
        }

        int16_t getCalibratedValue(ADC_DATA_TYPE sample_value){
            int data_milliVolts = 0;
            int16_t result = 0;
            auto err = adc_cali_raw_to_voltage(self->adc_cali_handle, sample_value, &data_milliVolts);
            if (err == ESP_OK) {
                result = static_cast<int16_t>(data_milliVolts);
            } else {
                LOGE("adc_cali_raw_to_voltage error: %d", err);
            }
            return result;
        }

        int getMinSamplesForAllChannels(int channels) {
            int result = 100000;
            for (int ch = 0; ch < channels; ch++){
                if (sampleIndex[ch] < result){
                    result = sampleIndex[ch];
                }
            }
            return result;
        }

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
        LOGI("sample rate (audio): %d", cfg.sample_rate);
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
