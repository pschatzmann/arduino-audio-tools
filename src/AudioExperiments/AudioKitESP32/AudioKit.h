/**
 * @file AudioKit.h
 * @author Phil Schatzmann
 * @brief A complete implementation for a ES8388 codec
 * @version 0.1
 * @date 2021-12-12
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#pragma once

#include <SPI.h>
#include <string.h>

#include "AudioI2S/I2SConfig.h"
#include "AudioI2S/I2SStream.h"
#include "AudioTools/AudioActions.h"
#include "AudioDevices/AudioKitESP32/AudioKitPins.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s.h"

#define KEY_RESPONSE_TIME_MS 10
#define HP_DELAY_TIME_MS 1000

#if USE_AUDIO_KIT == 2
// ai tinker board pins
// https://github.com/Ai-Thinker-Open/ESP32-A1S-AudioKit)
#include "AudioDevices/AudioKitESP32/ai-thinker.h"
#else
// lyrat pins:
// https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/board-esp32-lyrat-mini-v1.2.html#gpio-allocation-summary
#include "AudioDevices/AudioKitESP32/layrat.h"
#endif

#define ACK_CHECK_EN 0x1
#define ACK_CHECK_DIS 0x0
#define ACK_VAL I2C_MASTER_ACK
#define NACK_VAL I2C_MASTER_NACK

/* ES8388 register */
#define ES8388_CONTROL1 0x00
#define ES8388_CONTROL2 0x01
#define ES8388_CHIPPOWER 0x02
#define ES8388_ADCPOWER 0x03
#define ES8388_DACPOWER 0x04
#define ES8388_CHIPLOPOW1 0x05
#define ES8388_CHIPLOPOW2 0x06
#define ES8388_ANAVOLMANAG 0x07
#define ES8388_MASTERMODE 0x08
/* ADC */
#define ES8388_ADCCONTROL1 0x09
#define ES8388_ADCCONTROL2 0x0a
#define ES8388_ADCCONTROL3 0x0b
#define ES8388_ADCCONTROL4 0x0c
#define ES8388_ADCCONTROL5 0x0d
#define ES8388_ADCCONTROL6 0x0e
#define ES8388_ADCCONTROL7 0x0f
#define ES8388_ADCCONTROL8 0x10
#define ES8388_ADCCONTROL9 0x11
#define ES8388_ADCCONTROL10 0x12
#define ES8388_ADCCONTROL11 0x13
#define ES8388_ADCCONTROL12 0x14
#define ES8388_ADCCONTROL13 0x15
#define ES8388_ADCCONTROL14 0x16
/* DAC */
#define ES8388_DACCONTROL1 0x17
#define ES8388_DACCONTROL2 0x18
#define ES8388_DACCONTROL3 0x19
#define ES8388_DACCONTROL4 0x1a
#define ES8388_DACCONTROL5 0x1b
#define ES8388_DACCONTROL6 0x1c
#define ES8388_DACCONTROL7 0x1d
#define ES8388_DACCONTROL8 0x1e
#define ES8388_DACCONTROL9 0x1f
#define ES8388_DACCONTROL10 0x20
#define ES8388_DACCONTROL11 0x21
#define ES8388_DACCONTROL12 0x22
#define ES8388_DACCONTROL13 0x23
#define ES8388_DACCONTROL14 0x24
#define ES8388_DACCONTROL15 0x25
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL18 0x28
#define ES8388_DACCONTROL19 0x29
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL22 0x2c
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e
#define ES8388_DACCONTROL25 0x2f
#define ES8388_DACCONTROL26 0x30
#define ES8388_DACCONTROL27 0x31
#define ES8388_DACCONTROL28 0x32
#define ES8388_DACCONTROL29 0x33
#define ES8388_DACCONTROL30 0x34


namespace audio_tools {

// forward declaration
class AudioKitStream;

typedef enum {
  MIC_GAIN_MIN = -1,
  MIC_GAIN_0DB = 0,
  MIC_GAIN_3DB = 3,
  MIC_GAIN_6DB = 6,
  MIC_GAIN_9DB = 9,
  MIC_GAIN_12DB = 12,
  MIC_GAIN_15DB = 15,
  MIC_GAIN_18DB = 18,
  MIC_GAIN_21DB = 21,
  MIC_GAIN_24DB = 24,
  MIC_GAIN_MAX,
} es_mic_gain_t;

typedef enum {
  ES_MODE_MIN = -1,
  ES_MODE_SLAVE = 0x00,
  ES_MODE_MASTER = 0x01,
  ES_MODE_MAX,
} es_mode_t;

typedef enum {
  ES_MODULE_MIN = -1,
  ES_MODULE_ADC = 0x01,
  ES_MODULE_DAC = 0x02,
  ES_MODULE_ADC_DAC = 0x03,
  ES_MODULE_LINE = 0x04,
  ES_MODULE_MAX
} es_module_t;

typedef enum {
  BIT_LENGTH_MIN = -1,
  BIT_LENGTH_16BITS = 0x03,
  BIT_LENGTH_18BITS = 0x02,
  BIT_LENGTH_20BITS = 0x01,
  BIT_LENGTH_24BITS = 0x00,
  BIT_LENGTH_32BITS = 0x04,
  BIT_LENGTH_MAX,
} es_bits_length_t;

typedef enum {
  LCLK_DIV_MIN = -1,
  LCLK_DIV_128 = 0,
  LCLK_DIV_192 = 1,
  LCLK_DIV_256 = 2,
  LCLK_DIV_384 = 3,
  LCLK_DIV_512 = 4,
  LCLK_DIV_576 = 5,
  LCLK_DIV_768 = 6,
  LCLK_DIV_1024 = 7,
  LCLK_DIV_1152 = 8,
  LCLK_DIV_1408 = 9,
  LCLK_DIV_1536 = 10,
  LCLK_DIV_2112 = 11,
  LCLK_DIV_2304 = 12,

  LCLK_DIV_125 = 16,
  LCLK_DIV_136 = 17,
  LCLK_DIV_250 = 18,
  LCLK_DIV_272 = 19,
  LCLK_DIV_375 = 20,
  LCLK_DIV_500 = 21,
  LCLK_DIV_544 = 22,
  LCLK_DIV_750 = 23,
  LCLK_DIV_1000 = 24,
  LCLK_DIV_1088 = 25,
  LCLK_DIV_1496 = 26,
  LCLK_DIV_1500 = 27,
  LCLK_DIV_MAX,
} es_lclk_div_t;

typedef enum {
  MCLK_DIV_MIN = -1,
  MCLK_DIV_1 = 1,
  MCLK_DIV_2 = 2,
  MCLK_DIV_3 = 3,
  MCLK_DIV_4 = 4,
  MCLK_DIV_6 = 5,
  MCLK_DIV_8 = 6,
  MCLK_DIV_9 = 7,
  MCLK_DIV_11 = 8,
  MCLK_DIV_12 = 9,
  MCLK_DIV_16 = 10,
  MCLK_DIV_18 = 11,
  MCLK_DIV_22 = 12,
  MCLK_DIV_24 = 13,
  MCLK_DIV_33 = 14,
  MCLK_DIV_36 = 15,
  MCLK_DIV_44 = 16,
  MCLK_DIV_48 = 17,
  MCLK_DIV_66 = 18,
  MCLK_DIV_72 = 19,
  MCLK_DIV_5 = 20,
  MCLK_DIV_10 = 21,
  MCLK_DIV_15 = 22,
  MCLK_DIV_17 = 23,
  MCLK_DIV_20 = 24,
  MCLK_DIV_25 = 25,
  MCLK_DIV_30 = 26,
  MCLK_DIV_32 = 27,
  MCLK_DIV_34 = 28,
  MCLK_DIV_7 = 29,
  MCLK_DIV_13 = 30,
  MCLK_DIV_14 = 31,
  MCLK_DIV_MAX,
} es_sclk_div_t;

typedef enum {
  ES_I2S_MIN = -1,
  ES_I2S_NORMAL = 0,
  ES_I2S_LEFT = 1,
  ES_I2S_RIGHT = 2,
  ES_I2S_DSP = 3,
  ES_I2S_MAX
} es_i2s_fmt_t;

typedef enum {
  ADC_INPUT_MIN = -1,
  ADC_INPUT_LINPUT1_RINPUT1 = 0x00,
  ADC_INPUT_MIC1 = 0x05,
  ADC_INPUT_MIC2 = 0x06,
  ADC_INPUT_LINPUT2_RINPUT2 = 0x50,
  ADC_INPUT_DIFFERENCE = 0xf0,
  ADC_INPUT_MAX,
} es_adc_input_t;

typedef enum {
  DAC_OUTPUT_MIN = -1,
  DAC_OUTPUT_LOUT1 = 0x04,
  DAC_OUTPUT_LOUT2 = 0x08,
  DAC_OUTPUT_SPK = 0x09,
  DAC_OUTPUT_ROUT1 = 0x10,
  DAC_OUTPUT_ROUT2 = 0x20,
  DAC_OUTPUT_ALL = 0x3c,
  DAC_OUTPUT_MAX,
} es_codec_dac_output_t;

typedef enum {
  AUDIO_HAL_ADC_INPUT_LINE1 = 0x00, /*!< mic input to adc channel 1 */
  AUDIO_HAL_ADC_INPUT_LINE2,        /*!< mic input to adc channel 2 */
  AUDIO_HAL_ADC_INPUT_ALL,          /*!< mic input to both channels of adc */
  AUDIO_HAL_ADC_INPUT_DIFFERENCE,   /*!< mic input to adc difference channel */
} audio_hal_adc_input_t;

/**
 * @brief Select channel for dac output
 */
typedef enum {
  AUDIO_HAL_DAC_OUTPUT_LINE1 = 0x00, /*!< dac output signal to channel 1 */
  AUDIO_HAL_DAC_OUTPUT_LINE2,        /*!< dac output signal to channel 2 */
  AUDIO_HAL_DAC_OUTPUT_ALL,          /*!< dac output signal to both channels */
} audio_hal_dac_output_t;

typedef struct {
  es_sclk_div_t sclk_div; /*!< bits clock divide */
  es_lclk_div_t lclk_div; /*!< WS clock divide */
} es_i2s_clock_t;

/**
 * @brief Configuratiion for ES8388
 *
 */
struct ConfigES8388 : public I2SConfig {
 public:
  ConfigES8388() {
    pin_ws = PIN_I2S_AUDIO_KIT_WS;
    pin_bck = PIN_I2S_AUDIO_KIT_BCK;
    use_apll = true;
  }
  bool is_amplifier_active = true;
  int default_volume = 20;

  // we define separate data pins!
  int pin_data_out = PIN_I2S_AUDIO_KIT_DATA_OUT;
  int pin_data_in = PIN_I2S_AUDIO_KIT_DATA_IN;

  // i2c setup
  i2c_port_t i2c_master = I2C_MASTER_NUM;
  int pin_i2c_scl = I2C_MASTER_SCL_IO;
  int pin_i2c_sda = I2C_MASTER_SDA_IO;

  // Define final input or output device
  audio_hal_adc_input_t input_device = AUDIO_HAL_ADC_INPUT_LINE2;  // or ADC_INPUT_MIC2
  audio_hal_dac_output_t output_device = AUDIO_HAL_DAC_OUTPUT_ALL;
  es_i2s_clock_t *clock_config = nullptr;

  bool headphone_detection_active = true;
  bool mic_active = false;
  bool actions_active = true;
};

// Access for callbacks
static AudioKitStream *pt_AudioKitStream = nullptr;

/**
 * @brief ESP32 Audio Kit using the ESP8388 DAC and ADC
 *
 */
class AudioKitStream : public AudioStreamX {
 public:
  /// Default Constructor
  AudioKitStream() { pt_AudioKitStream = this; };

  /// Destructor
  ~AudioKitStream() { deinitES8388(); }

  /**
   * @brief Provides a default cofiguration object
   *
   * @param mode
   * @return ConfigES8388
   */
  ConfigES8388 defaultConfig(RxTxMode mode = TX_MODE) {
    ConfigES8388 c;
    c.rx_tx_mode = mode;
    return c;
  }

  /**
   * @brief Starts the output processing
   *
   * @return true
   * @return false
   */
  virtual bool begin() { return begin(defaultConfig()); }

  /**
   * @brief Starts the processing
   *
   * @param cfg
   * @return true
   * @return false
   */
  virtual bool begin(ConfigES8388 cfgPar) {
    LOGI(LOG_METHOD);
    bool result = true;
    cfg = cfgPar;
    bool isDac = cfg.rx_tx_mode == TX_MODE;
    bool isAdc = cfg.rx_tx_mode == RX_MODE;
    module_value = isDac ? ES_MODULE_DAC : ES_MODULE_ADC;

    // log configuration
    cfg.logInfo();
    LOGI("==> isDac %d / isAdc: %d ", isDac, isAdc);

    // prepare SPI for SD support: begin(sck,miso,mosi,ss);
    SPI.begin(PIN_AUDIO_KIT_SD_CARD_CLK, PIN_AUDIO_KIT_SD_CARD_MISO,
              PIN_AUDIO_KIT_SD_CARD_MOSI, PIN_AUDIO_KIT_SD_CARD_CS);

    if (cfg.actions_active || cfg.headphone_detection_active) {
      setupActions();
    }

    if (!initES8388(!cfg.is_master, cfg.output_device, cfg.input_device)) {
      result = false;
      LOGE("Error: initES8388 failed");
    }
    if (!configClock(cfg.clock_config)) {
      result = false;
      LOGE("Error: configClock failed");
    }
    if (!setBitsPerSample(module_value, cfg.bits_per_sample)) {
      result = false;
      LOGE("Error: setBitsPerSample failed");
    }
    if (!setFormat(module_value, cfg.i2s_format)) {
      result = false;
      LOGE("Error: setFormat failed");
    }

    if (cfg.rx_tx_mode == RX_MODE) {
      // determine input type
      uint8_t inputType = ADC_INPUT_MIC1; //ADC_INPUT_LINPUT1_RINPUT1;
      if (cfg.input_device == AUDIO_HAL_ADC_INPUT_LINE2) {
        inputType = ADC_INPUT_MIC2; //ADC_INPUT_LINPUT2_RINPUT2;
      }

      if (!configAdcInput(inputType)) {
        result = false;
        LOGE("Error: configAdcInput failed");
      }
    } else {
      // determine output type
      uint8_t outputType = DAC_OUTPUT_ALL;
      if (cfg.output_device == AUDIO_HAL_DAC_OUTPUT_LINE1) {
        outputType = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2;
      } else if (cfg.output_device == AUDIO_HAL_DAC_OUTPUT_LINE2) {
        outputType = DAC_OUTPUT_LOUT2 | DAC_OUTPUT_LOUT2;
      }

      if (!configDacOutput(outputType)) {
        result = false;
        LOGE("Error: configDacOutput failed");
      }
    }

    // set initial volume
    actionVolume = cfg.default_volume;
    if (!setVoiceVolume(actionVolume)) {
      result = false;
      LOGE("setVoiceVolume failed");
    }

    // start i2s
    i2s.begin(cfg, cfg.pin_data_out, cfg.pin_data_in);

    // configure clock line
    if (cfg.is_master) {
      i2s_mclk_gpio_select((i2s_port_t)cfg.port_no,
                           (gpio_num_t)PIN_I2S_AUDIO_KIT_MCLK);
    }

    // start module
    if (!start(module_value)) {
      LOGE("start failed");
      result = false;
    }

    // display all registers
    dumpRegisters();

    active = result;
    return result;
  }

  /**
   * @brief Ends the processing
   */
  virtual void end() {
    LOGI(LOG_METHOD);
    setPAPower(false);
    stop(module_value);
    active = false;
  }

  /// Writes the audio data to I2S
  virtual size_t write(const uint8_t *buffer, size_t size) {
    LOGD(LOG_METHOD);
    if (!active) {
      LOGE("you did not start the AudioKitStream with begin");
      return 0;
    }
    return i2s.writeBytes(buffer, size);
  }

  /// Reads the audio data
  virtual size_t readBytes(uint8_t *data, size_t length) override {
    if (!active) {
      LOGE("you did not start the AudioKitStream with begin");
      return 0;
    }
    return i2s.readBytes(data, length);
  }

  /// Provides the available audio data
  virtual int available() override { return i2s.available(); }

  /// Provides the available audio data
  virtual int availableForWrite() override { return i2s.availableForWrite(); }

  /**
   * @brief Reconfigure audio information
   *
   * @param info
   */
  virtual void setAudioInfo(AudioBaseInfo info) {
    LOGI(LOG_METHOD);
    info.logInfo();
    // update current cfg
    cfg.sample_rate = info.sample_rate;
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.channels = info.channels;
    // update bits per sample in ES8388
    setBitsPerSample(cfg.bits_per_sample);
    // reconfigure i2s
    i2s_set_clk((i2s_port_t)cfg.port_no, info.sample_rate,
                (i2s_bits_per_sample_t)info.bits_per_sample,
                (i2s_channel_t)info.channels);
  }

  /**
   * @brief Set ES8388 PA power
   *
   * @param enable true for enable PA power, false for disable PA power
   */
  void setPAPower(bool enable) {
    LOGI("setPAPower: %s", enable ? "true" : "false");
    actualPower = enable;
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64((gpio_num_t)PA_ENABLE_GPIO);
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    io_conf.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&io_conf);
    if (enable) {
      gpio_set_level((gpio_num_t)PA_ENABLE_GPIO, 1);
    } else {
      gpio_set_level((gpio_num_t)PA_ENABLE_GPIO, 0);
    }
  }

  /**
   * @brief Configure ES8388 ADC and DAC volume. Basicly you can consider this
   * as ADC and DAC gain
   *
   * @param mode:             set ADC or DAC or all
   * @param volume:           -96 ~ 0              for example
   * Es8388SetAdcDacVolume(ES_MODULE_ADC, 30, 6); means set ADC volume -30.5db
   * @param dot:              whether include 0.5. for example
   * Es8388SetAdcDacVolume(ES_MODULE_ADC, 30, 4); means set ADC volume -30db
   *
   */
  bool setVolume(int mode, int volume, int dot) {
    LOGI("setVolume(%d,%d,%d)", mode, volume, dot);
    int res = 0;
    if (volume < -96 || volume > 0) {
      LOGW("Warning: volume < -96! or > 0!\n");
      if (volume < -96)
        volume = -96;
      else
        volume = 0;
    }
    dot = (dot >= 5 ? 1 : 0);
    volume = (-volume << 1) + dot;
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
      res |= i2c_write_reg(ES8388_ADCCONTROL8, volume);
      // ADC Right Volume=0db
      res |= i2c_write_reg(ES8388_ADCCONTROL9, volume);
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
      res |= i2c_write_reg(ES8388_DACCONTROL5, volume);
      res |= i2c_write_reg(ES8388_DACCONTROL4, volume);
    }
    return res == 0;
  }

  /**
   * @brief  Set voice volume
   *
   * @param volume : 0 ~100 * *@ return *-(-1)Error * -(0)Success
   **/
  bool setVoiceVolume(int volume) {
    LOGI("setVoiceVolume: %d", volume);
    esp_err_t res = ESP_OK;
    if (volume < 0)
      volume = 0;
    else if (volume > 100)
      volume = 100;
    volume /= 3;
    res = i2c_write_reg(ES8388_DACCONTROL24, volume);
    res |= i2c_write_reg(ES8388_DACCONTROL25, volume);
    res |= i2c_write_reg(ES8388_DACCONTROL26, 0);
    res |= i2c_write_reg(ES8388_DACCONTROL27, 0);
    return res == ESP_OK;
  }

  /**
   *
   * @brief Get voice volume
   *  @return volume:  voice volume (0~100)
   */
  int voiceVolume() {
    LOGI(LOG_METHOD);
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    int volume;
    res = i2c_read_reg(ES8388_DACCONTROL24, &reg);
    if (res == ESP_FAIL) {
      volume = 0;
    } else {
      volume = reg;
      volume *= 3;
      if (volume == 99) volume = 100;
    }
    return volume;
  }

  /**
   * @brief Increments/Decrements the volume
   *
   * @param inc
   * @return true
   * @return false
   */
  void incrementVoiceVolume(int inc) {
    actionVolume += inc;
    setVoiceVolume(actionVolume);
  }

  /**
   * @brief Configure ES8388 DAC mute or not. Basically you can use this
   * function to mute the output or unmute
   *
   * @param enable: enable or disable
   *
   * @return
   *     - (-1) Parameter error
   *     - (0)   Success
   */
  bool setVoiceMute(bool enable) {
    LOGI("setVoiceMute: %s", enable ? "true" : "false");
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    res = i2c_read_reg(ES8388_DACCONTROL3, &reg);
    reg = reg & 0xFB;
    res |= i2c_write_reg(ES8388_DACCONTROL3, reg | (((int)enable) << 2));
    return res == ESP_OK;
  }

  /**
   * @brief Get ES8388 DAC mute status
   *
   *  @return bool
   *
   */

  bool isVoiceMute(void) {
    LOGI(LOG_METHOD);
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    res = i2c_read_reg(ES8388_DACCONTROL3, &reg);
    if (res == ESP_OK) {
      reg = (reg & 0x04) >> 2;
    }
    return reg;
  }

  /**
   * @param gain: see es_mic_gain_t
   *
   * @return
   *     - (-1) Parameter error
   *     - (0)   Success
   */
  bool setMicrophoneGain(es_mic_gain_t gain) {
    LOGI(LOG_METHOD);
    esp_err_t res, gain_n;
    gain_n = (int)gain / 3;
    gain_n = (gain_n << 4) + gain_n;
    // MIC PGA
    res = i2c_write_reg(ES8388_ADCCONTROL1, gain_n);
    return res == ESP_OK;
  }

  /**
   * @brief Starts or stops a module: ES_MODULE_ADC, ES_MODULE_LINE,
   * ES_MODULE_DAC, ES_MODULE_ADC_DAC
   * @param mode
   * @param active
   * @return int
   */
  int setStateActive(es_module_t mode, bool active) {
    LOGI(LOG_METHOD);
    int res = 0;
    es_module_t es_mode = mode;
    if (!active) {
      res = stop(es_mode);
    } else {
      res = start(es_mode);
      LOGD("start default is decode mode:%d", es_mode);
    }
    return res == ESP_OK;
  }

  /**
   * @brief Checks if the headphone is connected
   *
   * @return true connected
   * @return false not connected
   */
  bool headphoneStatus() {
    return !gpio_get_level((gpio_num_t)HEADPHONE_DETECT);
  }

  /**
   * @brief Process input keys and pins
   *
   */
  void processActions() {
    static unsigned long keys_timeout = 0;
    if (keys_timeout < millis()) {
      // LOGD(LOG_METHOD);
      if (cfg.actions_active || cfg.headphone_detection_active) {
        actions.processActions();
      }
      keys_timeout = millis() + KEY_RESPONSE_TIME_MS;
    }
    yield();
  }

  /**
   * @brief Defines a new action that is executed when the indicated pin is
   * active
   *
   * @param pin
   * @param action
   */
  void addAction(int pin, void (*action)()) {
    LOGI(LOG_METHOD);
    actions.add(pin, action);
  }

  /**
   * @brief Increase the volume
   *
   */
  static void actionVolumeUp() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->incrementVoiceVolume(+2);
  }

  /**
   * @brief Decrease the volume
   *
   */
  static void actionVolumeDown() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->incrementVoiceVolume(-2);
  }

  /**
   * @brief Toggle start stop
   *
   */
  static void actionStartStop() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->setPAPower(!pt_AudioKitStream->actualPower);
  }

  /**
   * @brief Start
   *
   */
  static void actionStart() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->setPAPower(true);
  }

  /**
   * @brief Stop
   *
   */
  static void actionStop() {
    LOGI(LOG_METHOD);
    pt_AudioKitStream->setPAPower(false);
  }

  /**
   * @brief Process headphone detection
   *
   */
  static void actionHeadphoneStatus() {
    LOGI("process headphone detection");
    bool isConnected = pt_AudioKitStream->headphoneStatus();
    bool powerActive = !isConnected;
    if (powerActive != pt_AudioKitStream->actualPower) {
      LOGW("Headphone jack has been %s", isConnected ? "inserted" : "removed");
      pt_AudioKitStream->setPAPower(powerActive);
    }
  }

 protected:
  ConfigES8388 cfg;
  es_module_t module_value;
  I2SBase i2s;
  AudioActions actions;
  int actionVolume = 0;
  bool actualPower;
  bool active;

  /// init i2c with different possible pins
  esp_err_t i2c_init(void) {
    LOGI("i2c sda: %d", cfg.pin_i2c_sda);
    LOGI("i2c scl: %d", cfg.pin_i2c_scl);
    esp_err_t result = i2c_init(I2C_MASTER_NUM, cfg.pin_i2c_sda, cfg.pin_i2c_scl);
    if (result != ESP_OK) {
      LOGE("I2C Init failed with configured pins %d/%d", cfg.pin_i2c_sda, cfg.pin_i2c_scl);
    }
    return result;
  }

  esp_err_t i2c_init(int port, int sda, int scl) {
    LOGD(LOG_METHOD);

    i2c_port_t i2c_master_port = cfg.i2c_master;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
  }

  esp_err_t i2c_deinit() {
    LOGD(LOG_METHOD);
    return i2c_driver_delete(cfg.i2c_master);
  }

  esp_err_t i2c_write_reg(uint8_t reg_add, uint8_t data) {
    return i2c_write(I2C_MASTER_ADDR, reg_add, data);
  }

  esp_err_t i2c_read_reg(uint8_t reg_add, uint8_t *p_data) {
    *p_data = i2c_read(I2C_MASTER_ADDR, reg_add);
    return ESP_OK;
  }

  uint8_t i2c_read(uint8_t i2c_bus_addr, uint8_t reg) {
    uint8_t buffer[2];
    // printf( "Addr: [%d] Reading register: [%d]\n", i2c_bus_addr, reg );

    buffer[0] = reg;

    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Write the register address to be read
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_bus_addr << 1 | I2C_MASTER_WRITE,
                          ACK_CHECK_EN);
    i2c_master_write_byte(cmd, buffer[0], ACK_CHECK_EN);

    // Read the data for the register from the slave
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_bus_addr << 1 | I2C_MASTER_READ,
                          ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &buffer[0], NACK_VAL);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(cfg.i2c_master, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return (buffer[0]);
  }

  uint8_t i2c_write(uint8_t i2c_bus_addr, uint8_t reg, uint8_t value) {
    return i2c_write_bulk(i2c_bus_addr, reg, 1, &value);
  }

  uint8_t i2c_write_bulk(uint8_t i2c_bus_addr, uint8_t reg, uint8_t bytes,
                         uint8_t *data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_bus_addr << 1 | I2C_MASTER_WRITE,
                          ACK_CHECK_EN);
    i2c_master_write(cmd, &reg, 1, ACK_CHECK_EN);
    i2c_master_write(cmd, data, bytes, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin((i2c_port_t)cfg.i2c_master, cmd,
                                   1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
      return ret;
    }
    return 0;
  }

  void dumpRegisters() {
    LOGD(LOG_METHOD);
    for (int i = 0; i < 50; i++) {
      uint8_t reg = 0;
      i2c_read_reg(i, &reg);
      LOGI("Register %d - (%x): %x - %s", i, i, reg, Str::toBinary(&reg, 1));
    }
  }

  bool initES8388(bool isMaster, audio_hal_dac_output_t dac,
                  audio_hal_adc_input_t adc) {
    LOGD(LOG_METHOD);
    int res = 0;

    res = i2c_init();  // ESP32 in master mode
    // 0x04 mute/0x00 unmute&ramp;DAC unmute and disabled digital volume control
    // soft ramp
    res |= i2c_write_reg(ES8388_DACCONTROL3, 0x04);
    // Chip Control and Power Management
    res |= i2c_write_reg(ES8388_CONTROL2, 0x50);
    // normal all and power up all
    res |= i2c_write_reg(ES8388_CHIPPOWER, 0x00);

    // Disable the internal DLL to improve 8K sample rate
    res |= i2c_write_reg(0x35, 0xA0);
    res |= i2c_write_reg(0x37, 0xD0);
    res |= i2c_write_reg(0x39, 0xD0);

    // CODEC IN I2S SLAVE MODE
    res |= i2c_write_reg(ES8388_MASTERMODE,
                         isMaster ? ES_MODE_MASTER : ES_MODE_SLAVE);

    // disable DAC and disable Lout/Rout/1/2
    res |= i2c_write_reg(ES8388_DACPOWER, 0xC0);
    // Enfr=0,Play&Record Mode,(0x17-both of mic&paly)
    res |= i2c_write_reg(ES8388_CONTROL2, 0);
    // LPVrefBuf=0,Pdn_ana=0
    res |= i2c_write_reg(ES8388_CONTROL1, 0x12);

    // DAC
    // 1a 0x18:16bit iis , 0x00:24
    res |= i2c_write_reg(ES8388_DACCONTROL1, 0x18);
    // DACFsMode,SINGLE SPEED; DACFsRatio,256
    res |= i2c_write_reg(ES8388_DACCONTROL2, 0x02);
    // 0x00 audio on LIN1&RIN1,
    res |= i2c_write_reg(ES8388_DACCONTROL16, 0x00);
    // only left DAC to left mixer enable 0db
    res |= i2c_write_reg(ES8388_DACCONTROL17, 0x90);
    // only right DAC to right mixer enable 0db
    res |= i2c_write_reg(ES8388_DACCONTROL20, 0x90);
    // set internal ADC and DAC use the same LRCK clock, ADC LRCK as internal
    // LRCK
    res |= i2c_write_reg(ES8388_DACCONTROL21, 0x80);
    // vroi=0
    res |= i2c_write_reg(ES8388_DACCONTROL23, 0x00);
    // no volume
    setVolume(ES_MODULE_DAC, 0, 0);

    int lrout = 0;
    if (AUDIO_HAL_DAC_OUTPUT_LINE2 == dac) {
      lrout = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_ROUT1;
    } else if (AUDIO_HAL_DAC_OUTPUT_LINE1 == dac) {
      lrout = DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT2;
    } else {
      lrout = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 |
              DAC_OUTPUT_ROUT2;
    }
    // 0x3c Enable DAC and Enable Lout/Rout/1/2
    res |= i2c_write_reg(ES8388_DACPOWER, lrout);

    // ADC
    res |= i2c_write_reg(ES8388_ADCPOWER, 0xFF);
    // MIC Left and Right channel PGA gain
    res |= i2c_write_reg(ES8388_ADCCONTROL1, 0xbb);

    // 0x00 LINSEL & RINSEL, LIN1/RIN1 as ADC Input; DSSEL,use one DS Reg11;
    lrout = 0;
    if (AUDIO_HAL_ADC_INPUT_LINE1 == adc) {
      lrout = ADC_INPUT_MIC1; //ADC_INPUT_LINPUT1_RINPUT1;
    } else if (AUDIO_HAL_ADC_INPUT_LINE2 == adc) {
      lrout = ADC_INPUT_MIC2; //ADC_INPUT_LINPUT2_RINPUT2;
    } else {
      lrout = ADC_INPUT_DIFFERENCE;
    }
    // / 0x00 LINSEL & RINSEL, LIN1/RIN1 as ADC Input;  DSSEL,use one DS Reg11;
    // DSR, LINPUT1-RINPUT1
    res |= i2c_write_reg(ES8388_ADCCONTROL2, lrout);

    res |= i2c_write_reg(ES8388_ADCCONTROL3, 0x02);
    // Left/Right data, Left/Right justified mode, Bits length, I2S format
    res |= i2c_write_reg(ES8388_ADCCONTROL4, 0x0d);
    // ADCFsMode,singel SPEED,RATIO=256
    res |= i2c_write_reg(ES8388_ADCCONTROL5, 0x02);

    // ALC for Microphone
    setVolume(ES_MODULE_ADC, 0, 0);  // no volume
    // Power on ADC, Enable LIN&RIN, Power off MICBIAS, set int1lp to low
    // power mode
    res |= i2c_write_reg(ES8388_ADCPOWER, 0x09);

    /* enable es8388 PA */
    setPAPower(cfg.is_amplifier_active);
    LOGI("init,out:%02x, in:%02x", dac, adc);
    return res == ESP_OK;
  }

  /**
   * @brief Deinitialize ES8388 codec chip
   *
   * @return
   *     - ESP_OK
   *     - ESP_FAIL
   */
  bool deinitES8388(void) {
    LOGD(LOG_METHOD);
    int res = 0;
    // reset and stop es8388
    res = i2c_write_reg(ES8388_CHIPPOWER, 0xFF);
    i2c_deinit();

    return res == ESP_OK;
  }

  /**
   * @brief Power Management
   *
   * @param mod:      if ES_POWER_CHIP, the whole chip including ADC and DAC is
   * enabled
   * @param enable:   false to disable true to enable
   *
   * @return
   *     - (-1)  Error
   *     - (0)   Success
   */
  bool start(es_module_t mode) {
    LOGD(LOG_METHOD);
    esp_err_t res = ESP_OK;
    uint8_t prev_data = 0, data = 0;
    i2c_read_reg(ES8388_DACCONTROL21, &prev_data);
    if (mode == ES_MODULE_LINE) {
      // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2 by pass enable
      res |= i2c_write_reg(ES8388_DACCONTROL16, 0x09);
      // left DAC to left mixer enable  and  LIN signal to left mixer enable 0db
      // : bupass enable
      res |= i2c_write_reg(ES8388_DACCONTROL17, 0x50);
      // right DAC to right mixer enable  and  LIN signal
      // to right mixer enable 0db : bupass enable
      res |= i2c_write_reg(ES8388_DACCONTROL20, 0x50);
      // enable adc
      res |= i2c_write_reg(ES8388_DACCONTROL21, 0xC0);
    } else {
      // enable dac
      res |= i2c_write_reg(ES8388_DACCONTROL21, 0x80);
    }

    i2c_read_reg(ES8388_DACCONTROL21, &data);
    if (prev_data != data) {
      // start state machine
      res |= i2c_write_reg(ES8388_CHIPPOWER, 0xF0);

      // res |= i2c_write_reg(ES8388_CONTROL1, 0x16);
      // res |= i2c_write_reg(ES8388_CONTROL2, 0x50);
      // start state machine
      res |= i2c_write_reg(ES8388_CHIPPOWER, 0x00);
    }
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC ||
        mode == ES_MODULE_LINE) {
      // power up adc and line in
      res |= i2c_write_reg(ES8388_ADCPOWER, 0x00);
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC ||
        mode == ES_MODULE_LINE) {
      // power up dac and line out
      res |= i2c_write_reg(ES8388_DACPOWER, 0x3c);

      setVoiceMute(false);
      LOGD( "start default is mode:%d", mode);
    }

    return res == ESP_OK;
  }

  /**
   * @brief Power Management
   *
   * @param mod:      if ES_POWER_CHIP, the whole chip including ADC and DAC is
   * enabled
   * @param enable:   false to disable true to enable
   *
   * @return
   *     - (-1)  Error
   *     - (0)   Success
   */
  bool stop(es_module_t mode) {
    LOGD(LOG_METHOD);
    esp_err_t res = ESP_OK;
    if (mode == ES_MODULE_LINE) {
      res |= i2c_write_reg(ES8388_DACCONTROL21, 0x80);  // enable
                                                        // dac
      // res |= i2c_write_reg(ES8388_DACCONTROL16,
      //                      0x00); // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
      res |= i2c_write_reg(ES8388_DACCONTROL17,
                           0x90);  // only left DAC to left mixer enable 0db
      res |= i2c_write_reg(ES8388_DACCONTROL20,
                           0x90);  // only right DAC to right mixer enable 0db
      return res;
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
      res |= i2c_write_reg(ES8388_DACPOWER, 0x00);
      setVoiceMute(true);
      // res |= Es8388SetAdcDacVolume(ES_MODULE_DAC, -96, 5); // 0db
      // res |= i2c_write_reg(ES8388_DACPOWER, 0xC0);  //power down
      // dac and line out
    }
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
      // res |= Es8388SetAdcDacVolume(ES_MODULE_ADC, -96, 5);      // 0db
      // power down adc and line in
      res |= i2c_write_reg(ES8388_ADCPOWER, 0xFF);
    }
    if (mode == ES_MODULE_ADC_DAC) {
      // disable mclk
      res |= i2c_write_reg(ES8388_DACCONTROL21, 0x9C);
      //        res |= i2c_write_reg(ES8388_CONTROL1, 0x00);
      //        res |= i2c_write_reg(ES8388_CONTROL2, 0x58);
      //        res |= i2c_write_reg(ES8388_CHIPPOWER, 0xF3);
      //        //stop state machine
    }

    return res == ESP_OK;
  }

  /**
   * @brief Config I2s clock in MSATER mode
   *
   * @param cfg.sclkDiv:      generate SCLK by dividing MCLK in MSATER mode
   * @param cfg.lclkDiv:      generate LCLK by dividing MCLK in MSATER mode
   *
   * @return
   *     - (-1)  Error
   *     - (0)   Success
   */
  bool configClock(es_i2s_clock_t *clock_config) {
    esp_err_t res = ESP_OK;
    if (clock_config != nullptr) {
      LOGI(LOG_METHOD);
      res |= i2c_write_reg(ES8388_MASTERMODE, (*clock_config).sclk_div);
      // ADCFsMode,singel SPEED,RATIO=256
      res |= i2c_write_reg(ES8388_ADCCONTROL5, (*clock_config).lclk_div);
      // ADCFsMode,singel SPEED,RATIO=256
      res |= i2c_write_reg(ES8388_DACCONTROL2, (*clock_config).lclk_div);
    } else {
      LOGD("no clock configured");
    }

    return res == ESP_OK;
  }

  /**
   * @brief setFormat
   * @param module
   * @param fmt ES_I2S_MIN, ES_I2S_NORMAL, ES_I2S_LEFT, ES_I2S_RIGHT, ES_I2S_DSP
   * @return true
   * @return false
   */
  bool setFormat(es_module_t module, I2SFormat fmt) {
    LOGD(LOG_METHOD);
    switch (fmt) {
      case I2S_STD_FORMAT:
        return setFormat(module, ES_I2S_NORMAL);
      case I2S_LSB_FORMAT:
        return setFormat(module, ES_I2S_LEFT);
      case I2S_MSB_FORMAT:
        return setFormat(module, ES_I2S_RIGHT);
      case I2S_PHILIPS_FORMAT:
        return setFormat(module, ES_I2S_NORMAL);
      case I2S_RIGHT_JUSTIFIED_FORMAT:
        return setFormat(module, ES_I2S_RIGHT);
      case I2S_LEFT_JUSTIFIED_FORMAT:
        return setFormat(module, ES_I2S_LEFT);
      case I2S_PCM_LONG:
        return setFormat(module, ES_I2S_DSP);
      case I2S_PCM_SHORT:
        return setFormat(module, ES_I2S_DSP);
    }
    return false;
  }
  /**
   * @brief Configure ES8388 I2S format
   *
   * @param mode:           set ADC or DAC or all
   * @param bitPerSample:   see Es8388I2sFmt
   *
   * @return
   *     - (-1) Error
   *     - (0)  Success
   */
  bool setFormat(es_module_t mode, es_i2s_fmt_t fmt) {
    LOGD(LOG_METHOD);
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
      res = i2c_read_reg(ES8388_ADCCONTROL4, &reg);
      reg = reg & 0xfc;
      res |= i2c_write_reg(ES8388_ADCCONTROL4, reg | fmt);
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
      res = i2c_read_reg(ES8388_DACCONTROL1, &reg);
      reg = reg & 0xf9;
      res |= i2c_write_reg(ES8388_DACCONTROL1, reg | (fmt << 1));
    }
    return res == ESP_OK;
  }

  /**
   * @param gain: Config DAC Output
   *
   * @return
   *     - (-1) Parameter error
   *     - (0)   Success
   */
  bool configDacOutput(uint8_t output) {
    LOGD(LOG_METHOD);
    esp_err_t res;
    uint8_t reg = 0;
    res = i2c_read_reg(ES8388_DACPOWER, &reg);
    reg = reg & 0xc3;
    res |= i2c_write_reg(ES8388_DACPOWER, reg | output);
    return res == ESP_OK;
  }

  /**
   * @param gain: Config ADC input
   *
   * @return
   *     - (-1) Parameter error
   *     - (0)   Success
   */
  bool configAdcInput(uint8_t input) {
    LOGD(LOG_METHOD);
    esp_err_t res;
    uint8_t reg = 0;
    res = i2c_read_reg(ES8388_ADCCONTROL2, &reg);
    reg = reg & 0x0f;
    res |= i2c_write_reg(ES8388_ADCCONTROL2, reg | input);
    return res == ESP_OK;
  }

  /**
   * @brief Configure ES8388 data sample bits
   * @param mode
   * @param bits_length (as int)
   * @return true
   * @return false
   */
  bool setBitsPerSample(es_module_t module, int bit_length) {
    LOGD(LOG_METHOD);
    switch (bit_length) {
      case 16:
        return setBitsPerSample(module, BIT_LENGTH_16BITS);
      case 18:
        return setBitsPerSample(module, BIT_LENGTH_18BITS);
      case 20:
        return setBitsPerSample(module, BIT_LENGTH_20BITS);
      case 24:
        return setBitsPerSample(module, BIT_LENGTH_24BITS);
      case 32:
        return setBitsPerSample(module, BIT_LENGTH_32BITS);
      default:
        LOGE("Unsupported bits_per_sample: %d", bit_length);
    }
    return false;
  }
  /**
   * @brief Configure ES8388 data sample bits
   *
   * @param mode:             set ADC or DAC or all
   * @param bitPerSample:   see BitsLength
   *
   * @return
   *     - (-1) Parameter error
   *     - (0)   Success
   */
  bool setBitsPerSample(es_module_t mode, es_bits_length_t bits_length) {
    LOGD(LOG_METHOD);
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    int bits = (int)bits_length;

    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
      res = i2c_read_reg(ES8388_ADCCONTROL4, &reg);
      reg = reg & 0xe3;
      res |= i2c_write_reg(ES8388_ADCCONTROL4, reg | (bits << 2));
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
      res = i2c_read_reg(ES8388_DACCONTROL1, &reg);
      reg = reg & 0xc7;
      res |= i2c_write_reg(ES8388_DACCONTROL1, reg | (bits << 3));
    }
    return res == ESP_OK;
  }

  /**
   * @brief Set the Bits Per Sample for the ES_MODULE_ADC_DAC
   *
   * @param bitsPerSample
   * @return true
   * @return false
   */
  bool setBitsPerSample(int bitsPerSample) {
    LOGD(LOG_METHOD);
    esp_err_t res = ESP_OK;
    es_bits_length_t tmp;
    // res |= setFormat(ES_MODULE_ADC_DAC, iface->fmt);
    if (bitsPerSample == 16) {
      tmp = BIT_LENGTH_16BITS;
    } else if (bitsPerSample == 24) {
      tmp = BIT_LENGTH_24BITS;
    } else {
      tmp = BIT_LENGTH_32BITS;
    }
    res |= setBitsPerSample(ES_MODULE_ADC_DAC, tmp);
    return res == ESP_OK;
  }

  /**
   * @brief Set Masterclock GPIO only pin 0,1 and 2 a supported
   *
   * @param i2s_num
   * @param gpio_num
   * @return esp_err_t
   */
  esp_err_t i2s_mclk_gpio_select(i2s_port_t i2s_num, gpio_num_t gpio_num) {
    if (i2s_num >= I2S_NUM_MAX) {
      LOGE( "Does not support i2s number(%d)", i2s_num);
      return ESP_ERR_INVALID_ARG;
    }
    if (gpio_num != GPIO_NUM_0 && gpio_num != GPIO_NUM_1 &&
        gpio_num != GPIO_NUM_3) {
      LOGE( "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", gpio_num);
      return ESP_ERR_INVALID_ARG;
    }
    LOGI("I2S%d, MCLK output by GPIO%d", i2s_num, gpio_num);
    if (i2s_num == I2S_NUM_0) {
      if (gpio_num == GPIO_NUM_0) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
      } else if (gpio_num == GPIO_NUM_1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
        WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
      } else {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
        WRITE_PERI_REG(PIN_CTRL, 0xFF00);
      }
    } else if (i2s_num == I2S_NUM_1) {
      if (gpio_num == GPIO_NUM_0) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        WRITE_PERI_REG(PIN_CTRL, 0xFFFF);
      } else if (gpio_num == GPIO_NUM_1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
        WRITE_PERI_REG(PIN_CTRL, 0xF0FF);
      } else {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
        WRITE_PERI_REG(PIN_CTRL, 0xFF0F);
      }
    }
    return ESP_OK;
  }

  /**
   * @brief Setup the supported default actions
   *
   */
  void setupActions() {
    LOGI(LOG_METHOD);
    if (cfg.headphone_detection_active) {
      actions.add(HEADPHONE_DETECT, actionHeadphoneDetection,
                  AudioActions::ActiveChange);
    }
    actions.add(PIN_KEY1, actionStartStop);
    actions.add(PIN_KEY5, actionVolumeDown);
    actions.add(PIN_KEY6, actionVolumeUp);
  }
};

// Not sure yet which name is best
typedef AudioKitStream AudioKit;
typedef AudioKitStream ESP32AudioKit;
typedef AudioKitStream ESP8388Stream;

}  // namespace audio_tools