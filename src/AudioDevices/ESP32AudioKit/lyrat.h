#pragma once

// As documented in https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/board-esp32-lyrat-mini-v1.2.html#gpio-allocation-summary
// I do not have any device, so I could not test this

#define I2C_MASTER_SCL_IO 23     /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 18     /*!< gpio number for I2C master data  */

#define PIN_I2S_AUDIO_KIT_MCLK 0
#define PIN_I2S_AUDIO_KIT_BCK 5
#define PIN_I2S_AUDIO_KIT_WS 25
#define PIN_I2S_AUDIO_KIT_DATA_OUT 35 
#define PIN_I2S_AUDIO_KIT_DATA_IN 26

#define HEADPHONE_DETECT 19
#define PA_ENABLE_GPIO 21
