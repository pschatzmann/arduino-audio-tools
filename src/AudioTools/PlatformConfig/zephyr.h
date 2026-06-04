#pragma once


#include <zephyr/sys/util.h>

#define IS_ZEPHYR
#define NO_INPLACE_INIT_SUPPORT
#define USE_TIMER
#define USE_PWM
//#define USE_TYPETRAITS

#if defined(CONFIG_I2S)
#  define USE_I2S
#endif

#if defined(CONFIG_DAC) || defined(CONFIG_ADC)
#  define USE_ANALOG
#endif

#if IS_ENABLED(CONFIG_DAC)
#  define USE_ANALOG_DAC
#endif

#if IS_ENABLED(CONFIG_ADC)
#  define USE_ANALOG_ADC
#endif

// Enable PSRAM allocator if the external memory heap is available
#if defined(CONFIG_HEAP_MEM_POOL_SIZE) && CONFIG_HEAP_MEM_POOL_SIZE > 0
#  define USE_PSRAM
#endif
