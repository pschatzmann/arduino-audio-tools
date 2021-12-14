#pragma once

// Audio Kit V2.3 

/* ES8388 address */
#define I2C_MASTER_NUM I2C_NUM_0 /*!< I2C port number for master dev */
#define I2C_MASTER_ADDR  0x20
#define I2C_MASTER_SCL_IO 32     
#define I2C_MASTER_SDA_IO 33    

// Some 2.2 board use the the above settings and some the following:
//#define I2C_MASTER_SCL_IO 18    
//#define I2C_MASTER_SDA_IO 23    

#define PIN_I2S_AUDIO_KIT_MCLK 0
#define PIN_I2S_AUDIO_KIT_BCK 27
#define PIN_I2S_AUDIO_KIT_WS 25
#define PIN_I2S_AUDIO_KIT_DATA_OUT 26
#define PIN_I2S_AUDIO_KIT_DATA_IN 35
// Headphone detect on 5, 19 or 39
#define HEADPHONE_DETECT 39 
#define PA_ENABLE_GPIO 21
