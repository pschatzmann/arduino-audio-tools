#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/List.h"

namespace audio_tools {

/**
 * @brief FIFO Queue which is based on a List that is thread 
 * save.
 * @ingroup collections
 * @author Phil Schatzmann
 * @copyright GPLv3
 * @tparam T
 * @tparam TMutex
 */
template <class T, class TMutex>
class SynchronizedQueue {
 public:
  SynchronizedQueue() = default;

  bool enqueue(T& data) {
    LockGuard(mutex);
    return l.push_front(data);
  }

  bool peek(T& data) {
    LockGuard(mutex);
    if (l.end()->prior == nullptr) return false;
    data = *(l.end()->prior);
    return true;
  }

  bool dequeue(T& data) {
    LockGuard(mutex);
    return l.pop_back(data);
  }

  size_t size() {
    LockGuard(mutex);
    return l.size();
  }

  bool clear() {
    LockGuard(mutex);
    return l.clear();
  }

  bool empty() {
    LockGuard(mutex);
    return l.empty();
  }

  void setAllocator(Allocator& allocator) { l.setAllocator(allocator); }

 protected:
  List<T> l;
  TMutex mutex;
};

}  // namespace audio_tools