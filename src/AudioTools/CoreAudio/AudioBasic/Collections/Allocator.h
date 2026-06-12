#pragma once
#include <stdlib.h>

#include "AudioToolsConfig.h"
#include "AudioTools/CoreAudio/AudioLogger.h"
// Some top level functions: stop(), checkMemory()
#include "AudioTools/CoreAudio/AudioRuntime.h"

namespace audio_tools {

/**
 * @defgroup memorymgmt Memory Management
 * @ingroup tools
 * @brief Allocators and Memory Manager
 */

/**
 * @brief Memory allocateator which uses malloc.
 * @ingroup memorymgmt
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class Allocator {
 public:
  // creates an object
  template <class T>
  T* create() {
    void* addr = allocate(sizeof(T));
    // call constructor
    T* ref = new (addr) T();
    return ref;
  }

  /// deletes an object
  template <class T>
  void remove(T* obj) {
    if (obj == nullptr) return;
    obj->~T();
    free((void*)obj);
  }

  // creates an array of objects
  template <class T>
  T* createArray(int len) {
    void* addr = allocate(sizeof(T) * len);
    T* addrT = (T*)addr;
    // call constructor
#ifndef NO_INPLACE_INIT_SUPPORT
    for (int j = 0; j < len; j++) new (addrT + j) T();
#else
    T default_value;
    for (int j = 0; j < len; j++) {
      memcpy((uint8_t*)addr + (j * sizeof(T)), &default_value, sizeof(T));
    }
#endif
    return (T*)addr;
  }

  // deletes an array of objects
  template <class T>
  void removeArray(T* obj, int len) {
    if (obj == nullptr) return;
    for (int j = 0; j < len; j++) {
      obj[j].~T();
    }
    free((void*)obj);
  }

  /// Allocates memory
  virtual void* allocate(size_t size) {
    void* result = do_allocate(size);
    if (result == nullptr) {
      LOGE("Allocateation failed for %zu bytes", size);
      stop();
    } else {
      LOGD("Allocated %zu", size);
    }
    return result;
  }

  /// frees memory
  virtual void free(void* memory) {
    if (memory != nullptr) ::free(memory);
  }

 protected:
  virtual void* do_allocate(size_t size) {
    return calloc(1, size == 0 ? 1 : size);
  }
};

/**
 * @brief Memory allocateator which uses ps_malloc (on the ESP32) and if this
 * fails it resorts to malloc.
 * @ingroup memorymgmt
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AllocatorExt : public Allocator {
  void* do_allocate(size_t size) {
    void* result = nullptr;
    if (size == 0) size = 1;
#if defined(USE_PSRAM) && defined(ARDUINO)
    result = ps_malloc(size);
#endif
    if (result == nullptr) result = malloc(size);
    if (result == nullptr) {
      LOGE("allocateation failed for %zu bytes", size);
      stop();
    }
    // initialize object
    memset(result, 0, size);
    return result;
  }
};

#if (defined(ESP32)) && defined(ARDUINO)

/**
 * @brief ESP32 Memory allocateator which forces the allocation with the defined
 * attributes (default MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL)
 * @ingroup memorymgmt
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class AllocatorESP32 : public Allocator {
 public:
  AllocatorESP32(int caps = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL) {
    this->caps = caps;
  }
  void* do_allocate(size_t size) {
    void* result = nullptr;
    if (size == 0) size = 1;
    result = heap_caps_calloc(1, size, caps);
    if (result == nullptr) {
      LOGE("alloc failed for %zu bytes", size);
      stop();
    }
    return result;
  }

 protected:
  int caps = 0;
};

static AllocatorESP32 DefaultESP32AllocatorRAM;

#endif

#if defined(USE_PSRAM) && defined(ARDUINO)

/**
 * @brief Memory allocateator which uses ps_malloc to allocate the memory in
 * PSRAM on the ESP32
 * @ingroup memorymgmt
 * @author Phil Schatzmann
 * @copyright GPLv3
 **/

class AllocatorPSRAM : public Allocator {
  void* do_allocate(size_t size) {
    if (size == 0) size = 1;
    void* result = nullptr;
    result = ps_calloc(1, size);
    if (result == nullptr) {
      LOGE("allocateation failed for %zu bytes", size);
      stop();
    }
    return result;
  }
};

#endif

#if defined(IS_ZEPHYR)
#include <zephyr/kernel.h>
#if IS_ENABLED(CONFIG_ESP_SPIRAM)
#include <zephyr/sys/multi_heap_shared.h>
#endif


/**
 * @brief Zephyr allocator using the kernel heap (k_malloc / k_free).
 * This is the standard allocator for Zephyr builds.
 * @ingroup memorymgmt
 */
class AllocatorZephyr : public Allocator {
 public:
  void* do_allocate(size_t size) override {
    if (size == 0) size = 1;
    void* result = k_malloc(size);
    if (result != nullptr) memset(result, 0, size);
    return result;
  }

  void free(void* memory) override {
    if (memory != nullptr) k_free(memory);
  }
};

#if defined(USE_PSRAM) && IS_ENABLED(CONFIG_ESP_SPIRAM)
#include <zephyr/mem_mgmt/mem_attr_heap.h>

/**
 * @brief Zephyr PSRAM allocator using mem_attr_heap (requires
 * CONFIG_MEM_ATTR_HEAP=y and a memory region tagged with DT_MEM_ARM_MPU_RAM_NOCACHE
 * or similar PSRAM-backed attribute in the board DTS).
 * Falls back to the regular kernel heap if the PSRAM allocation fails.
 * @ingroup memorymgmt
 */
class AllocatorZephyrPSRAM : public Allocator {
 public:
  void* do_allocate(size_t size) override {
    if (size == 0) size = 1;
    void* result = nullptr;
#if IS_ENABLED(CONFIG_ESP_SPIRAM)
    result = shared_multi_heap_alloc(SMH_REG_ATTR_EXTERNAL,size,K_NO_WAIT);
#endif

    if (result == nullptr) result = k_malloc(size);  // fallback to system heap
    if (result != nullptr) memset(result, 0, size);
    return result;
  }

  void free(void* memory) override {
    // mem_attr_heap_free handles both PSRAM and fallback k_malloc allocations
    if (memory != nullptr) mem_attr_heap_free(memory);
  }
};

static AllocatorZephyrPSRAM DefaultAllocator;
static AllocatorZephyr DefaultAllocatorRAM;

#  else

static AllocatorZephyr DefaultAllocator;
static AllocatorZephyr DefaultAllocatorRAM;

#  endif  // USE_PSRAM / CONFIG_MEM_ATTR_HEAP

#else

static AllocatorExt DefaultAllocator;
static Allocator DefaultAllocatorRAM;

#endif  // IS_ZEPHYR


}  // namespace audio_tools
