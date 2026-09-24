/**
 * @file AudioRuntime.cpp
 * @author Phil Schatzmann
 * @brief Some platform specific exceptional things which can't be implemented in a header
 * @version 0.1
 * @date 2022-02-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "AudioToolsConfig.h"

#if defined(ARDUINO_ARCH_RP2040) && defined(FIX_SYNC_SYNCHRONIZE)
extern "C" void __sync_synchronize(){
}
#endif


#if defined(ARDUINO_ARCH_TANGNANO20K)
// The core links without libc: provide the function which is used by assert()
extern "C" __attribute__((weak)) void __assert_func(const char *file, int line,
                                                    const char *func,
                                                    const char *expr) {
  printf("assert failed: %s:%d %s: %s\n", file, line, func, expr);
  while (true);
}
#endif
