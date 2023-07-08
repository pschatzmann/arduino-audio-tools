#pragma once
#include <stddef.h>
#include <stdint.h>

namespace audio_tools {

/**
 * @brief Helps to split up a big memory array into smaller slices. There are no
 * additinal heap allocations!
 * Example: if we have an array with 9 entries (1,2,3,4,5,6,7,8,9): slices(5) gives 2.
 * slice(5,0) returns size 5 with 1,2,3,4,5 and slice(5,1) returns size 4 with 6,7,8,9!
 * @ingroup collections
 * @author Phil Schatzmann
 */
template <class T> 
class Slice {
 public:
  Slice(T* start, size_t len) {
    this->start = start;
    this->len = len;
  }

  /// Returns the data
  T* data() { return start; }

  /// Returns the (result) data size is number of entries
  size_t size() { return len; }

  /// Returns the number of slices of the indicated size
  size_t slices(int sliceSize){
    int result = len / sliceSize;
    return len % sliceSize == 0 ? result : result+1;
  }

  /// Returns true if we contain any valid data
  operator bool() { return len > 0 && start!=nullptr; }

  /// Returns the slice at the indicated index for the indicated slize size;
  Slice slice(int sliceSize, int idx) {
    int start_pos = idx * sliceSize;
    int end_pos = start_pos + sliceSize;
    if (end_pos > len) {
      end_pos = len;
    }

    if (start_pos < len) {
      assert(start!=nullptr);
      return Slice(start + start_pos, end_pos - start_pos);
    } else {
      return Slice(nullptr, 0);
    }
  }

 protected:
  T* start = nullptr;
  size_t len = 0;
};

}  // namespace audio_tools