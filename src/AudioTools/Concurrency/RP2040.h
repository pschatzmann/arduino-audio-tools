#pragma once
#include "AudioTools/Concurrency/Mutex.h"
#include "AudioTools/Concurrency/SynchronizedBuffer.h"
#include "AudioTools/Concurrency/SynchronizedQueue.h"
#if defined(FREERTOS_H) || defined(INC_FREERTOS_H)
#  include "RTOS.h" 
#  define USE_FREETROS
#else
#  include "AudioTools/Concurrency/RP2040/BufferRP2040.h"
#  include "AudioTools/Concurrency/RP2040/MutexRP2040.h"
#endif

