#pragma once

#define USE_NANO33BLE 
#define USE_INT24_FROM_INT
#define USE_I2S
#define USE_PWM
#define USE_TYPETRAITS
#define USE_TIMER
//#define USE_INITIALIZER_LIST
#define USE_ALT_PIN_SUPPORT

#define PIN_PWM_START 5
#define PIN_I2S_BCK 2
#define PIN_I2S_WS 3
#define PIN_I2S_DATA_IN 4
#define PIN_I2S_DATA_OUT 4
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS SS
