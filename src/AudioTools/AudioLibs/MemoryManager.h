#pragma once

#include "AudioLogger.h"
#ifdef ESP32
#include "esp_heap_caps.h"
#endif

namespace audio_tools {

/**
 * @brief MemoryManager which activates the use of external SPIRAM memory.
 * When external memory is in use, the allocation strategy is to initially try
 * to satisfy smaller allocation requests with internal memory and larger requests
 * with external memory. This sets the limit between the two, as well as generally
 * enabling allocation in external memory.
 * @ingroup memorymgmt
 */
class MemoryManager {
public:
  /// Default Constructor - call begin() to activate PSRAM
  MemoryManager() = default;
  /// Constructor which activates PSRAM. This constructor automatically calls begin()
  MemoryManager(int limit) {
    begin(limit);
  };
  /// Activate the PSRAM for allocated memory > limit 
  bool begin(int limit = 10000) {
#ifdef ESP32
    LOGI("Activate PSRAM from %d bytes", limit);
    heap_caps_malloc_extmem_enable(limit);
    return true;
#else
    return false;
#endif
  }
};

}
