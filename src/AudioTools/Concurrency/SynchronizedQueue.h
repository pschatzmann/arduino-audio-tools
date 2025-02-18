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
    LockGuard guard{mutex};;
    return l.push_front(data);
  }

  bool peek(T& data) {
    LockGuard guard{mutex};;
    if (l.end()->prior == nullptr) return false;
    data = *(l.end()->prior);
    return true;
  }

  bool dequeue(T& data) {
    LockGuard guard{mutex};;
    return l.pop_back(data);
  }

  size_t size() {
    LockGuard guard{mutex};;
    return l.size();
  }

  bool clear() {
    LockGuard guard{mutex};;
    return l.clear();
  }

  bool empty() {
    LockGuard guard{mutex};;
    return l.empty();
  }

  void setAllocator(Allocator& allocator) { l.setAllocator(allocator); }

 protected:
  List<T> l;
  TMutex mutex;
};

}  // namespace audio_tools