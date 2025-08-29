#pragma once

#define RP2040_MBED

// install https://github.com/pschatzmann/rp2040-i2s and set USE_I2S to 1
#ifndef USE_I2S
#  define USE_I2S 0
#endif

#define USE_PWM
#define USE_ANALOG_ARDUINO
#define USE_TYPETRAITS
#define USE_TIMER
#define USE_INT24_FROM_INT

#define PIN_ANALOG_START 26
#define PIN_PWM_START 6
#define PIN_I2S_BCK 26
#define PIN_I2S_WS PIN_I2S_BCK+1
#define PIN_I2S_DATA_IN 28
#define PIN_I2S_DATA_OUT 28
// Default Setting: The mute pin can be switched actovated by setting it to a gpio (e.g 4). Or you could drive the LED by assigning LED_BUILTIN
#define PIN_I2S_MUTE -1
#define SOFT_MUTE_VALUE 0
#define PIN_CS 1 //PIN_SPI0_SS

// fix missing __sync_synchronize symbol
#define FIX_SYNC_SYNCHRONIZE
#define IRAM_ATTR

#ifndef ANALOG_BUFFER_SIZE 
#  define ANALOG_BUFFER_SIZE 1024
#endif

#ifndef ANALOG_BUFFERS 
#  define ANALOG_BUFFERS 50
#endif