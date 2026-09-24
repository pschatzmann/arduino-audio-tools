#pragma once

// Sipeed Tang Nano 20K (RISC-V softcore, zephyr-sdk toolchain):
// int32_t is defined as long, so we need a dedicated int constructor
#define USE_INT24_FROM_INT

// I2S: onboard MAX98357A; pins are defined by the gateware
#define USE_I2S
// ring buffer of 512 stereo 16 bit frames: this fits into the fast SRAM
#undef I2S_BUFFER_SIZE
#define I2S_BUFFER_SIZE 512
#undef I2S_BUFFER_COUNT
#define I2S_BUFFER_COUNT 4

#define USE_TIMER

// PWM: with Tools > PWM Audio: Enabled we use the PWMAudio hardware on
// GPIO16/GPIO17, otherwise analogWrite() on up to 6 pins starting at GPIO0
#define USE_PWM
#define PIN_PWM_START 14
#define PIN_PWM_COUNT 6
#undef PWM_AUDIO_FREQUENCY
#define PWM_AUDIO_FREQUENCY 50000

// There is not FPU
#define PREFER_FIXEDPOINT true
