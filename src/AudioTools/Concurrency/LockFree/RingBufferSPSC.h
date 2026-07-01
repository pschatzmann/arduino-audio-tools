#pragma once
#include <atomic>
#include <cstring>

#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Lock-free Single-Producer Single-Consumer ring buffer.
 *
 * Safe to use from two cores / contexts simultaneously **only** when there is
 * exactly one writer and exactly one reader — e.g. an ISR or a second core
 * filling the buffer while the main loop drains it (USB audio RX path on
 * RP2040 with core-1 tud_task).
 *
 * Design
 * ──────
 * - head_ is the byte-count of everything ever written; only the producer
 *   writes it (release store) and the consumer reads it (acquire load).
 * - tail_ is the byte-count of everything ever read;  only the consumer
 *   writes it (release store) and the producer reads it (acquire load).
 * - No shared mutable counter between the two sides — the shared mutable
 *   state of RingBuffer<T>::_numElems is the classic data race on M0+.
 * - Capacity is rounded up to the next power of two so that index masking
 *   replaces modulo and wrap-around is handled by a single bit-AND.
 * - readArray / writeArray use memcpy for bulk transfers and split them at
 *   the wrap-around point, giving at most two memcpy calls per operation.
 *
 * Not suitable for MPMC (multiple producers or consumers); use
 * QueueLockFree for that.
 *
 * @tparam T Element type. Works for any trivially-copyable T; the bulk-copy
 *           optimisation is most effective for T=uint8_t (audio streams).
 * @ingroup buffers
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename T = uint8_t>
class RingBufferSPSC : public BaseBuffer<T> {
 public:
  RingBufferSPSC() = default;

  explicit RingBufferSPSC(size_t capacity) { resize(capacity); }

  // ── BaseBuffer interface ────────────────────────────────────────────────

  bool write(T data) override { return writeArray(&data, 1) == 1; }

  bool read(T& result) override { return readArray(&result, 1) == 1; }

  bool peek(T& result) override {
    size_t h = head_.load(std::memory_order_acquire);
    size_t t = tail_.load(std::memory_order_relaxed);
    if (h == t) return false;
    result = buf_[t & mask_];
    return true;
  }

  // Bulk write — called only by the producer.
  int writeArray(const T data[], int len) override {
    if (capacity_ == 0 || len <= 0) return 0;
    size_t t   = tail_.load(std::memory_order_acquire);
    size_t h   = head_.load(std::memory_order_relaxed);
    size_t space = capacity_ - (h - t);
    size_t n   = (size_t)len < space ? (size_t)len : space;
    if (n == 0) return 0;

    size_t h_idx = h & mask_;
    size_t first = capacity_ - h_idx;        // bytes until end of physical buffer
    if (first > n) first = n;
    memcpy(buf_ + h_idx, data,          first * sizeof(T));
    memcpy(buf_,         data + first, (n - first) * sizeof(T));

    head_.store(h + n, std::memory_order_release);
    return (int)n;
  }

  // Bulk read — called only by the consumer.
  int readArray(T data[], int len) override {
    if (capacity_ == 0 || len <= 0) return 0;
    size_t h   = head_.load(std::memory_order_acquire);
    size_t t   = tail_.load(std::memory_order_relaxed);
    size_t avail = h - t;
    size_t n   = (size_t)len < avail ? (size_t)len : avail;
    if (n == 0) return 0;

    size_t t_idx = t & mask_;
    size_t first = capacity_ - t_idx;        // bytes until end of physical buffer
    if (first > n) first = n;
    memcpy(data,          buf_ + t_idx, first * sizeof(T));
    memcpy(data + first,  buf_,        (n - first) * sizeof(T));

    tail_.store(t + n, std::memory_order_release);
    return (int)n;
  }

  // available() is called from both sides (consumer AND feedback ISR on
  // producer core), so we use acquire on both loads for a consistent
  // snapshot regardless of which core is calling.
  int available() override {
    size_t h = head_.load(std::memory_order_acquire);
    size_t t = tail_.load(std::memory_order_acquire);
    return (int)(h - t);
  }

  int availableForWrite() override {
    size_t h = head_.load(std::memory_order_acquire);
    size_t t = tail_.load(std::memory_order_acquire);
    return (int)(capacity_ - (h - t));
  }

  // reset() is only safe when neither producer nor consumer is active
  // (e.g. USB bus reset).  It is not atomic.
  void reset() override {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  bool resize(size_t capacity) override {
    // Round up to the next power of two so masking replaces modulo.
    size_t pow2 = 1;
    while (pow2 < capacity) pow2 <<= 1;
    delete[] buf_;
    buf_      = (capacity > 0) ? new T[pow2] : nullptr;
    capacity_ = (capacity > 0) ? pow2 : 0;
    mask_     = capacity_ > 0  ? capacity_ - 1 : 0;
    reset();
    return true;
  }

  T*     address() override { return buf_; }
  size_t size()    override { return capacity_; }

  ~RingBufferSPSC() override { delete[] buf_; }

  // Non-copyable: the buffer owns heap memory and the atomics are not
  // copyable.
  RingBufferSPSC(const RingBufferSPSC&)            = delete;
  RingBufferSPSC& operator=(const RingBufferSPSC&) = delete;

 private:
  T*     buf_      = nullptr;
  size_t capacity_ = 0;
  size_t mask_     = 0;

  // Separate the two hot atomics onto different 32-byte regions to prevent
  // false sharing on multi-core targets that have a data cache (e.g. ESP32).
  // On Cortex-M0+ (RP2040, no cache) this is free.
  alignas(32) std::atomic<size_t> head_{0};  // written by producer only
  alignas(32) std::atomic<size_t> tail_{0};  // written by consumer only
};

}  // namespace audio_tools
