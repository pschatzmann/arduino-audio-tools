#pragma once
#include <atomic>
#include <cstring>

#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
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
 * - head_ is the element-count of everything ever written; only the
 *   producer writes it (release store) and the consumer reads it (acquire
 *   load).
 * - tail_ is the element-count of everything ever read; only the consumer
 *   writes it (release store) and the producer reads it (acquire load).
 * - No shared mutable counter between the two sides — the shared mutable
 *   state of RingBuffer<T>::_numElems is the classic data race on M0+.
 * - Capacity is rounded up to the next power of two so that index masking
 *   replaces modulo and wrap-around is handled by a single bit-AND.
 * - readArray / writeArray use memcpy for bulk transfers and split them at
 *   the wrap-around point, giving at most two memcpy calls per operation
 *   (T=uint8_t makes this a byte copy; for other T it's an element copy).
 *
 * Not suitable for MPMC (multiple producers or consumers); use
 * QueueLockFree for that.
 *
 * Backing storage is a Vector<T>, using Vector's own default allocator
 * (plain heap - unaffected, existing callers like the I2S/USB audio TX/RX
 * buffers see no change at all). setUsePSRAM(true), called before the
 * first resize(), switches it to an allocator that prefers PSRAM and
 * falls back to the plain heap silently where PSRAM isn't available -
 * useful for a buffer sized to soak up bursty backlogs (see e.g.
 * PacedVideoOutput) where a few hundred KB would be far too much to ask
 * of internal RAM but is a rounding error against multiple MB of PSRAM.
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

    T* buf = buf_.data();
    size_t h_idx = h & mask_;
    size_t first = capacity_ - h_idx;        // bytes until end of physical buffer
    if (first > n) first = n;
    memcpy(buf + h_idx, data,          first * sizeof(T));
    memcpy(buf,         data + first, (n - first) * sizeof(T));

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

    T* buf = buf_.data();
    size_t t_idx = t & mask_;
    size_t first = capacity_ - t_idx;        // bytes until end of physical buffer
    if (first > n) first = n;
    memcpy(data,          buf + t_idx, first * sizeof(T));
    memcpy(data + first,  buf,        (n - first) * sizeof(T));

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

  // Opt-in only - see the class comment. Call before the first resize();
  // changing it after buf_ is already allocated only takes effect on the
  // *next* resize() (the current allocation is left exactly as it is).
  void setUsePSRAM(bool flag) { use_psram_ = flag; }

  // NOT safe to call while the producer or consumer side may still be
  // active - same requirement as reset() above, and for the same reason:
  // it reallocates buf_ and touches capacity_/mask_ with no atomics/
  // barriers of its own. Callers must ensure both sides are quiescent
  // first (e.g. USB bus reset before either endpoint is re-armed).
  bool resize(size_t capacity) override {
    // RingBufferSPSC always discards its logical content on resize() (the
    // reset() call below), so release any existing allocation - under
    // whichever allocator it was actually made with - before switching
    // allocators or reallocating. This guarantees a block is never freed
    // through a different allocator instance than the one that allocated
    // it (which setUsePSRAM() toggling between resize() calls would
    // otherwise risk), and avoids Vector wastefully copying bytes forward
    // across the resize that are about to be discarded anyway. It also
    // means resize(0) actually releases the buffer instead of leaving the
    // old allocation in place (Vector::resize(0) alone is a no-op).
    buf_.reset();

    if (capacity == 0) {
      capacity_ = 0;
      mask_ = 0;
      reset();
      return true;
    }

    // Round up to the next power of two so masking replaces modulo, using
    // the same bounded bit-smear technique as QueueLockFree::resize()
    // rather than a naive `while (pow2 < capacity) pow2 <<= 1;` - that
    // loop never terminates once capacity exceeds the largest
    // representable power of two (pow2 overflows to 0 and the condition
    // stays true forever). The smear instead comes out as 0 in that case,
    // which is detected and rejected below.
    size_t pow2 = capacity - 1;
    for (size_t i = 1; i <= sizeof(size_t) * 4; i <<= 1) pow2 |= pow2 >> i;
    pow2 += 1;
    if (pow2 == 0) {
      capacity_ = 0;
      mask_ = 0;
      reset();
      return false;
    }

    buf_.setAllocator(use_psram_ ? DefaultAllocator : DefaultAllocatorRAM);
    bool allocated = buf_.resize(pow2) && buf_.data() != nullptr;
    capacity_ = allocated ? pow2 : 0;
    mask_     = capacity_ > 0 ? capacity_ - 1 : 0;
    reset();
    return allocated;
  }

  T*     address() override { return buf_.data(); }
  size_t size()    override { return capacity_; }

  ~RingBufferSPSC() override = default;

  // Non-copyable: the atomics are not copyable.
  RingBufferSPSC(const RingBufferSPSC&)            = delete;
  RingBufferSPSC& operator=(const RingBufferSPSC&) = delete;

 private:
  Vector<T> buf_;
  size_t capacity_ = 0;
  size_t mask_     = 0;
  bool   use_psram_ = false;

  // Separate the two hot atomics onto different 32-byte regions to prevent
  // false sharing on multi-core targets that have a data cache (e.g. ESP32).
  // On Cortex-M0+ (RP2040, no cache) this is free.
  alignas(32) std::atomic<size_t> head_{0};  // written by producer only
  alignas(32) std::atomic<size_t> tail_{0};  // written by consumer only
};

}  // namespace audio_tools
