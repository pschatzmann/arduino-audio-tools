#pragma once
#include <atomic>
#include <string.h>
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/AudioTypes.h"
#include "AudioTools/Concurrency/LockFree/RingBufferSPSC.h"
#include "AudioTools/Video/Video.h"

#ifdef __linux__
#include "AudioTools/Concurrency/Desktop/Task.h"
#else
#include "AudioTools/Concurrency/RTOS/Task.h"
#endif

namespace audio_tools {

/**
 * @brief Buffers a small, configurable amount of video (see
 * setQueueBytes()) and renders it frame by frame from a dedicated
 * background task, timed against an audio clock - so a demuxer's own
 * dispatch loop never blocks on video pacing. Wrap the real VideoOutput
 * (e.g. H264Decoder) in this and pass it to setOutputVideo() instead of
 * the decoder itself.
 *
 * write() requires one whole frame per call, immediately followed by
 * flush() (a no-op) - every current caller (DemuxerAVI/DemuxerMP4/
 * DemuxerMPG) already does this. Frame N renders once the audio clock
 * (setAudioClock(), defaults to wall clock) reaches N / fps (setFps());
 * setSchedulingDelayMs() corrects for the audio output's own buffering
 * latency.
 *
 * decode+render is assumed to keep up with the recording frame rate *on
 * average*. When it falls behind, write() drops non-keyframes instead of
 * blocking - once the queue is full, or proactively once the render task
 * is more than setCatchUpThresholdFrames() frame periods late (see
 * write()'s own comment). Keyframes are never dropped this way.
 *
 * Dropping alone can only prevent the backlog from growing, not shrink
 * it (the producer can't deliver frames faster than real time either).
 * setResyncThresholdMs(), setResyncQueueFillFraction(), and
 * setMaxQueuedIFrames() are the fallback: once any fires, taskLoop()
 * gives up on the backlog - keeping the freshest keyframe found in it
 * (see drainQueueKeepingLastKeyframe()) and jumping the schedule forward
 * to render it - so playback recovers (a visible jump) instead of
 * lagging indefinitely. Any P-frame after a resync is discarded until
 * the next real keyframe arrives, since a P-frame may reference a
 * picture the decoder no longer holds.
 *
 * Thread-safety: write() (the caller's thread) is the sole producer, the
 * background task the sole consumer of the frame queue - a lock-free
 * RingBufferSPSC, not a mutex.
 *
 * Diagnostics: frameCount()/frameCountI()/frameCountP()/
 * droppedFrameCount()/droppedIFrameCount(), avgFrameMs()/avgIFrameMs()/
 * avgPFrameMs(), inputFPS()/outputFPS(), queuedBytes()/
 * queuedIFrameCount(). Also logs its own processing (LOGI lifecycle,
 * LOGD per-frame, LOGW when falling behind/dropping).
 * @ingroup video
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class PacedVideoOutput : public VideoOutput {
 public:
  /// @param target every frame is eventually forwarded here (write() +
  /// flush()), from the background task only - never from the caller's
  /// own write()/flush() calls.
  /// @param fps nominal frames/second frames are scheduled against - 0
  /// (the default) means "not yet known"; set it later via setFps() (e.g.
  /// once the demuxer's parsed VideoInfo::fps becomes available) before
  /// the first write().
  /// @param schedulingDelayMs see setSchedulingDelayMs() - 0 (the
  /// default) applies no correction.
  PacedVideoOutput(VideoOutput& target, float fps = 0,
                      uint32_t schedulingDelayMs = 0)
      : p_target(&target), scheduling_delay_ms(schedulingDelayMs) {
    setFps(fps);
  }

  /// Sets/overrides the nominal frame rate frames are scheduled against -
  /// safe to call anytime, including before begin().
  void setFps(float fps) { frame_period_ms = fps > 0 ? 1000.0f / fps : 0; }

  /// Provides the audio clock frames are scheduled against - its
  /// playbackTime() (not millis(), see TimeSource's own comment for the
  /// difference) must return elapsed audio playback time; a ready-made
  /// implementation is AudioTimeSourceStream, AudioTools/CoreAudio/
  /// AudioIO.h. Must outlive this object. Leave unset to schedule against
  /// the wall clock instead.
  void setAudioClock(TimeSource& clock) { p_clock = &clock; }

  /// Corrects for the real audio output's own buffering latency: an
  /// AudioTimeSourceStream clock advances as soon as bytes are *accepted*
  /// by write(), not when they become audible - e.g. ~100ms ahead on a
  /// device with ~100ms of internal buffering, which would otherwise show
  /// each frame that much early. Delaying every frame's schedule by this
  /// amount (added to target_ms, only ever later, never earlier) cancels
  /// it out. No way to derive this automatically - pick it to roughly
  /// match your audio output's configured buffering latency; 0 (the
  /// default) applies no correction. Also settable via the constructor.
  void setSchedulingDelayMs(uint32_t delayMs) { scheduling_delay_ms = delayMs; }

  /// How many frame periods behind schedule the render task must fall
  /// before write() starts proactively dropping non-keyframes to catch up
  /// (see write()'s own comment), rather than only dropping once the
  /// queue is actually full. 1.0 (the default) drops once the most
  /// recently rendered frame was at least one frame period late; lower
  /// catches up faster at the cost of more drops, higher tolerates more
  /// backlog. No effect until setFps() reports a nonzero rate.
  void setCatchUpThresholdFrames(float frames) {
    catch_up_threshold_frames = frames;
  }

  /// Unconditionally drops every non-keyframe in write() itself, before
  /// it's even queued - codec-agnostic (works for H264Decoder/MPGDecoder/
  /// MJPEGDecoder/MultiVideoDecoder alike, unlike a decoder-specific flag
  /// such as MPGDecoder::setIgnorePFrames()) and cheaper than letting the
  /// decoder no-op an ignored frame after it: the bytes never get copied
  /// into the queue or scheduled at all. Off by default. See
  /// ignoredFrameCount() for how many this has skipped. Independent of
  /// setCatchUpThresholdFrames()'s own conditional dropping, which still
  /// applies when this is off.
  void setIgnorePFrames(bool active) { ignore_p_frames = active; }
  bool ignorePFrames() const { return ignore_p_frames; }

  /// How far behind schedule (ms) the render task must fall before it
  /// stops trying to catch up and instead jumps the schedule anchor
  /// forward to the current clock time (see taskLoop()) - dropping frames
  /// alone can't reduce a backlog, only slow its growth, since the
  /// producer can't deliver frames faster than real time. Content in the
  /// skipped gap is never shown - a visible jump, but playback recovers
  /// instead of lagging further. 2000ms (the default); 0 disables
  /// resyncing.
  void setResyncThresholdMs(uint32_t ms) { resync_threshold_ms = ms; }

  /// Byte-occupancy fraction (0..1) of the queue that triggers the same
  /// resync as setResyncThresholdMs(), off a different signal: dropped
  /// P-frames can keep the most recently rendered frame's own lateness
  /// low even while newer, not-yet-rendered bytes keep piling up, since
  /// overall render throughput can still trail the arrival rate. 0.8 (80%
  /// full, the default) catches that before the queue_full drop condition
  /// (100%) would otherwise be the only thing bounding it. 0 disables
  /// this trigger.
  void setResyncQueueFillFraction(float fraction) {
    resync_queue_fill_fraction = fraction;
  }

  /// Number of not-yet-consumed keyframes in the queue that triggers the
  /// same resync as the other two thresholds - catches a backlog earlier
  /// than either: several GOPs' worth of unconsumed keyframes already
  /// means none of them are worth rendering in order (see
  /// drainQueueKeepingLastKeyframe(), shared by all three triggers, for
  /// how the freshest one gets kept instead). 3 (the default) is
  /// deliberately small; 0 disables this trigger.
  void setMaxQueuedIFrames(int count) { max_queued_i_frames = count; }

  /// Stack size (words)/priority/core for the background render task -
  /// call before begin().
  void setTaskParameters(uint32_t stackSizeWords, uint8_t priority,
                         int core = -1) {
    task_stack_size = stackSizeWords;
    task_priority = priority;
    task_core = core;
  }

  /// Byte capacity of the frame queue - default 32KB. Too small behaves
  /// like a 1-frame queue (any one-off slow frame blocks write()
  /// immediately); larger absorbs transient jitter at the cost of RAM
  /// (and video trailing decode by however many frames end up buffered).
  /// Does not change what happens under a *sustained* mismatch: the
  /// queue still eventually fills, just later. Actual capacity may round
  /// up to the next power of two (see RingBufferSPSC). Call before
  /// begin()/the first frame - not supported afterwards.
  void setQueueBytes(size_t bytes) { queue_bytes = bytes > 0 ? bytes : 1; }

  /// Opts the frame queue into PSRAM-backed allocation instead of
  /// internal heap (see RingBufferSPSC::setUsePSRAM()) - falls back
  /// silently on boards without PSRAM. Worth enabling once
  /// setQueueBytes() is sized in the hundreds of KB+, since internal heap
  /// is a scarcer shared resource on most ESP32 boards. Call before
  /// begin()/the first frame - no effect on an already-allocated queue.
  void setQueueUsePSRAM(bool flag) { queue_use_psram = flag; }

  /// Starts the background render task - optional: write() calls this
  /// itself (idempotently) on first use if you never do, so the common
  /// case needs no explicit begin() at all. Call it yourself only if you
  /// need setTaskParameters()/setQueueBytes() to take effect (call those
  /// first) before any frame arrives.
  bool begin() {
    frame_index = 0;
    frame_count = 0;
    dropped_frame_count = 0;
    dropped_i_frame_count = 0;
    ignored_frame_count = 0;
    queued_i_frame_count = 0;
    logged_drop_burst = false;
    awaiting_keyframe = false;
    render_lateness_ms = 0;
    i_frame_count = 0;
    p_frame_count = 0;
    i_frame_total_ms = 0;
    p_frame_total_ms = 0;
    start_set = false;
    input_start_set = false;
    output_start_set = false;
    have_header = false;
    frame_buf.clear();
    queue.setUsePSRAM(queue_use_psram);
    if (queue_bytes_allocated != queue_bytes) {
      queue.resize(queue_bytes);
      queue_bytes_allocated = queue_bytes;
    } else {
      queue.reset();
    }
    task.create("PacedVideoOutput", task_stack_size, task_priority,
                task_core);
    bool ok = task.begin([this]() { taskLoop(); });
    task_started = task_started || ok;
    LOGI(
        "PacedVideoOutput: %s (fps=%.2f, clock=%s, scheduling delay=%u ms, "
        "queue=%u bytes, stack=%u words, priority=%u)",
        ok ? "render task started" : "render task failed to start",
        frame_period_ms > 0 ? 1000.0f / frame_period_ms : 0.0f,
        p_clock != nullptr ? "external audio" : "wall",
        (unsigned)scheduling_delay_ms, (unsigned)queue.size(),
        (unsigned)task_stack_size, (unsigned)task_priority);
    return ok;
  }

  /// Stops the background render task.
  void end() {
    task.end();
    task_started = false;
    LOGI(
        "PacedVideoOutput: render task stopped (%u frames rendered - %u "
        "I / %u P, %u P dropped / %u I dropped, input=%.2f fps, "
        "output=%.2f fps)",
        (unsigned)frame_count, (unsigned)i_frame_count.load(),
        (unsigned)p_frame_count.load(), (unsigned)dropped_frame_count,
        (unsigned)dropped_i_frame_count, inputFPS(), outputFPS());
  }

  /// Enqueues one complete frame. Unlike the general VideoOutput contract
  /// (write() may be called several times per frame, finalized by
  /// flush()), this class requires the whole frame in one call - every
  /// current caller (DemuxerAVI/DemuxerMP4/DemuxerMPG) already does this;
  /// a producer that split a frame across multiple write() calls would
  /// have each piece enqueued as its own separate, undecodable "frame" -
  /// undetected.
  ///
  /// Tags the frame with its scheduled presentation time (frame_index /
  /// fps), classifies it I vs P (see isKeyFrame()), and appends header +
  /// bytes to the background task's queue. Never renders inline - only
  /// the background task's write()+flush() into the real target does
  /// that, once the frame's scheduled time arrives.
  ///
  /// @param data the complete frame's bytes - copied before this call
  /// returns, so the caller can reuse/discard the buffer immediately.
  /// @param len number of bytes in `data`; 0 is a harmless no-op.
  /// @return `len`, always - even for a dropped frame (see
  /// droppedFrameCount()), so the caller's own bookkeeping sees no
  /// difference.
  size_t write(const uint8_t* data, size_t len) override {
    if (len == 0) return 0;
    if (!task_started) begin();
    if (!input_start_set) {
      input_start_ms = clockMs();
      input_start_set = true;
    }
    uint32_t this_frame = frame_index;
    uint32_t target_ms = (uint32_t)((double)frame_index * frame_period_ms);
    frame_index++;
    bool is_key = isKeyFrame(data, len);

    // See setIgnorePFrames() - unconditional, independent of queue/
    // lateness state, and checked before the header/queue-space bookkeeping
    // below since a dropped-here frame never touches any of that. Counts
    // into droppedFrameCount() too (same externally visible effect as any
    // other dropped P-frame - never rendered), with ignoredFrameCount()
    // as the more specific breakdown of how many were dropped for this
    // reason rather than backlog.
    if (ignore_p_frames && !is_key) {
      ignored_frame_count++;
      dropped_frame_count++;
      frame_count++;
      return len;
    }

    FrameHeader header;
    header.size = (uint32_t)len;
    header.target_ms = target_ms;
    header.is_key = is_key ? 1 : 0;
    size_t needed = sizeof(header) + len;

    // decode+render isn't keeping up right now. A non-keyframe is
    // dropped outright instead of enqueued (see droppedFrameCount()) to
    // catch back up without blocking this call, which would otherwise
    // stall the caller's own thread (typically a demuxer's dispatch
    // loop, delaying audio too). Two triggers: the queue is actually
    // full, or the render task's most recently rendered frame was
    // already more than setCatchUpThresholdFrames() frame periods late.
    // Keyframes are never dropped here (losing one would desync every
    // dependent P-frame) - if the queue can't fit one, this still blocks
    // below.
    bool queue_full = (size_t)queue.availableForWrite() < needed;
    bool falling_behind =
        frame_period_ms > 0 &&
        render_lateness_ms.load() >
            (int32_t)(catch_up_threshold_frames * frame_period_ms);
    if (!is_key && (queue_full || falling_behind)) {
      dropped_frame_count++;
      frame_count++;
      // Rate-limited to one line per unbroken run of drops - this can
      // trigger on every P-frame in a row for a long stretch, and
      // logging each one individually is expensive enough (Serial I/O on
      // this thread) to measurably worsen the backlog it's trying to
      // fix.
      if (!logged_drop_burst) {
        LOGW(
            "PacedVideoOutput: dropping P frames starting at #%u - %s "
            "(%d/%u bytes free, need %u, render %d ms behind)",
            (unsigned)this_frame,
            queue_full ? "render queue full" : "catching up",
            queue.availableForWrite(), (unsigned)queue.size(),
            (unsigned)needed, (int)render_lateness_ms.load());
        logged_drop_burst = true;
      }
      return len;
    }
    logged_drop_burst = false;

    bool logged_wait = false;
    while ((size_t)queue.availableForWrite() < needed) {
      if (!logged_wait) {
        LOGW(
            "PacedVideoOutput: write() blocked on frame #%u - render "
            "queue full (%d/%u bytes free, need %u)",
            (unsigned)this_frame, queue.availableForWrite(),
            (unsigned)queue.size(), (unsigned)needed);
        logged_wait = true;
      }
      delay(1);
    }
    queue.writeArray((const uint8_t*)&header, sizeof(header));
    queue.writeArray(data, len);
    if (is_key) queued_i_frame_count++;
    LOGD(
        "PacedVideoOutput: queued frame #%u (%s, %u bytes, target=%u ms, "
        "%d bytes free)",
        (unsigned)this_frame, is_key ? "I" : "P", (unsigned)len,
        (unsigned)target_ms, queue.availableForWrite());
    frame_count++;
    return len;
  }

  /// No-op: write() (see its own comment) already does all the work as
  /// soon as one complete frame arrives - kept only because every current
  /// caller still calls this unconditionally right after write().
  void flush() override {}

  void setSkipRender(bool skip) override { p_target->setSkipRender(skip); }

  /// Total number of frames handed off via write() so far - includes
  /// dropped frames (see droppedFrameCount()), since those still count as
  /// "handed off", just not enqueued.
  uint32_t frameCount() const { return frame_count; }

  /// Number of P-frames write() dropped instead of enqueueing - queue
  /// was full, the render task had fallen behind schedule (see write()'s
  /// own comment), or setIgnorePFrames() is on (see ignoredFrameCount()
  /// for that specific subset). 0 under normal operation with
  /// setIgnorePFrames() off; a rising count otherwise means decode+render
  /// can't sustain the recording frame rate - each dropped frame is
  /// simply never shown, the next queued one still renders at its own
  /// correct time, so this doesn't desync the audio/video clock.
  uint32_t droppedFrameCount() const { return dropped_frame_count; }

  /// Number of this stream's droppedFrameCount() specifically caused by
  /// setIgnorePFrames() rather than backlog (queue-full/falling-behind) -
  /// always 0 unless that's on.
  uint32_t ignoredFrameCount() const { return ignored_frame_count; }

  /// Number of keyframes discarded by a resync instead of being
  /// rendered. 0 under normal operation - unlike droppedFrameCount(),
  /// this only ever comes from a resync, never from write()'s own
  /// proactive check.
  uint32_t droppedIFrameCount() const { return dropped_i_frame_count; }

  /// Number of keyframes currently queued, not yet consumed (rendered or
  /// dropped) - see setMaxQueuedIFrames(). 0 or 1 under normal operation;
  /// a rising count is an early warning sign of a backlog.
  int queuedIFrameCount() const { return queued_i_frame_count.load(); }

  /// Bytes currently sitting in the queue, waiting for the render task.
  /// 0 most of the time under normal operation; a consistently non-zero
  /// value is an early warning sign before write() has to block. Compare
  /// against queueCapacityBytes() for a fill-level percentage.
  size_t queuedBytes() { return (size_t)queue.available(); }

  /// Total byte capacity of the frame queue - may be larger than
  /// requested (rounded up to the next power of two, see
  /// RingBufferSPSC). 0 before the queue has ever been allocated.
  size_t queueCapacityBytes() { return queue.size(); }

  /// Number of I-frames (key frames) actually rendered so far - see
  /// isKeyFrame() for how a frame is classified.
  uint32_t frameCountI() const { return i_frame_count; }
  /// Number of P-frames (non-key frames) actually rendered so far.
  uint32_t frameCountP() const { return p_frame_count; }

  /// Average time (ms) spent in the target's write()+flush() call (i.e.
  /// decode+render) for I-frames only, since begin().
  float avgIFrameMs() const {
    uint32_t n = i_frame_count;
    return n > 0 ? (float)i_frame_total_ms.load() / n : 0.0f;
  }
  /// Average time (ms) spent in the target's write()+flush() call for
  /// P-frames only, since begin().
  float avgPFrameMs() const {
    uint32_t n = p_frame_count;
    return n > 0 ? (float)p_frame_total_ms.load() / n : 0.0f;
  }
  /// Average time (ms) spent in the target's write()+flush() call across
  /// all rendered frames (I and P combined), since begin().
  float avgFrameMs() const {
    uint32_t n = i_frame_count + p_frame_count;
    return n > 0 ? (float)(i_frame_total_ms + p_frame_total_ms) / n : 0.0f;
  }

  /// Rate (frames/sec) frames are being written in, averaged from the
  /// first write() up to *now* - drops visibly if whatever feeds write()
  /// stalls, instead of freezing. Measured against setAudioClock()'s
  /// clock if set, wall clock otherwise - see clockMs(). Compare against
  /// outputFPS(): input consistently higher means the queue is filling
  /// up before write() has to block.
  float inputFPS() {
    if (!input_start_set) return 0.0f;
    uint32_t elapsed = clockMs() - input_start_ms;
    return elapsed > 0 ? (1000.0f * frame_count) / elapsed : 0.0f;
  }

  /// Rate (frames/sec) frames are actually being rendered - same
  /// averaging as inputFPS(), counting the background task's completed
  /// render calls. Always measured against wall-clock time regardless of
  /// setAudioClock(): answers "how fast can this hardware render", which
  /// shouldn't depend on audio's clock (playbackTime() freezes with any
  /// audio stall, which would otherwise distort this into an artificial
  /// spike on resume).
  float outputFPS() {
    if (!output_start_set) return 0.0f;
    uint32_t elapsed = millis() - output_start_ms.load();
    uint32_t count = i_frame_count + p_frame_count;
    return elapsed > 0 ? (1000.0f * count) / elapsed : 0.0f;
  }

  /// Prints a human-readable summary of the diagnostics above (fps,
  /// average render time, frame/drop counts, queue fill level, and on
  /// ESP32 also heap/PSRAM usage) to `out` - handy for a periodic status
  /// line in a sketch, e.g. `videoOutput.logTo(Serial);`.
  void logTo(Print& out) {
    out.print("input fps: ");
    out.print(inputFPS());
    out.print(" / output fps: ");
    out.println(outputFPS());
    out.print("avg render ms - I: ");
    out.print(avgIFrameMs());
    out.print(" / P: ");
    out.print(avgPFrameMs());
    out.print(" / overall: ");
    out.println(avgFrameMs());
    out.print("frames - I: ");
    out.print(frameCountI());
    out.print(" / P: ");
    out.print(frameCountP());
    out.print(" / dropped P: ");
    out.print(droppedFrameCount());
    out.print(" / dropped I: ");
    out.print(droppedIFrameCount());
    out.print(" / queued I: ");
    out.print(queuedIFrameCount());
    size_t queueCapacity = queueCapacityBytes();
    out.print(" / queue: ");
    out.print((unsigned)queuedBytes());
    out.print("/");
    out.print((unsigned)queueCapacity);
    out.print(" bytes (");
    out.print(queueCapacity > 0 ? 100.0f * queuedBytes() / queueCapacity
                                 : 0.0f);
    out.println("% full)");
#ifdef ESP32
    size_t heapFree = ESP.getFreeHeap();
    size_t heapTotal = ESP.getHeapSize();
    size_t heapUsed = heapTotal - heapFree;

    size_t psramFree = ESP.getFreePsram();
    size_t psramTotal = ESP.getPsramSize();
    size_t psramUsed = psramTotal - psramFree;

    char buf[80];
    snprintf(buf, sizeof(buf), "Heap:  total=%u, used=%u, free=%u bytes",
             (unsigned)heapTotal, (unsigned)heapUsed, (unsigned)heapFree);
    out.println(buf);
    snprintf(buf, sizeof(buf), "PSRAM: total=%u, used=%u, free=%u bytes",
             (unsigned)psramTotal, (unsigned)psramUsed, (unsigned)psramFree);
    out.println(buf);
#endif
  }

 protected:
  VideoOutput* p_target;
  TimeSource* p_clock = nullptr;
  float frame_period_ms = 0;
  uint32_t scheduling_delay_ms = 0;  // see setSchedulingDelayMs()
  float catch_up_threshold_frames = 1.0f;  // see setCatchUpThresholdFrames()
  bool ignore_p_frames = false;  // see setIgnorePFrames()
  uint32_t resync_threshold_ms = 2000;  // see setResyncThresholdMs()
  float resync_queue_fill_fraction = 0.8f;  // see setResyncQueueFillFraction()
  int max_queued_i_frames = 4;  // see setMaxQueuedIFrames()
  uint64_t frame_index = 0;
  uint32_t frame_count = 0;
  uint32_t dropped_frame_count = 0;  // see droppedFrameCount()
  uint32_t dropped_i_frame_count = 0;  // consumer-thread-only; see droppedIFrameCount()
  uint32_t ignored_frame_count = 0;  // write()-thread-only; see ignoredFrameCount()
  // Keyframes currently queued, not yet consumed - incremented by
  // write(), decremented by taskLoop()/drainQueueKeepingLastKeyframe();
  // atomic since producer and consumer both touch it. See
  // queuedIFrameCount().
  std::atomic<int32_t> queued_i_frame_count{0};
  bool logged_drop_burst = false;  // write()-thread-only; see write()
  uint32_t start_ms = 0;
  bool start_set = false;
  bool task_started = false;
  // True from the moment a resync fires until a real keyframe renders -
  // consumer-thread-only. A resync can leave the decoder's
  // reference-frame state unreliable, so P-frames are discarded until a
  // self-contained keyframe resets it.
  bool awaiting_keyframe = false;
  // How many ms late (or early) the most recently rendered frame was -
  // written by taskLoop(), read by write() (its proactive drop check),
  // hence atomic. A heuristic: stale by one frame's processing time by
  // the time write() reads it, same as output_start_ms below.
  std::atomic<int32_t> render_lateness_ms{0};

  /// Fixed-size POD record prefixed to each frame's bytes in 'queue' -
  /// RingBufferSPSC itself is just a raw byte stream with no concept of
  /// "one message", so write()/taskLoop() build one on top of it.
  struct FrameHeader {
    uint32_t size = 0;
    uint32_t target_ms = 0;
    uint8_t is_key = 0;
  };
  /// Lock-free single-producer (write())/single-consumer (taskLoop())
  /// byte queue - see setQueueBytes(). Safe without a mutex specifically
  /// because there is exactly one writer and one reader (see the class
  /// comment) - RingBufferSPSC is not safe for more than that.
  RingBufferSPSC<uint8_t> queue;
  size_t queue_bytes = 32 * 1024;      // desired capacity - see setQueueBytes()
  size_t queue_bytes_allocated = 0;    // what 'queue' was last resize()d to
  bool queue_use_psram = false;        // see setQueueUsePSRAM()

  // Consumer-side-only framing state (see taskLoop()): a header already
  // pulled out of 'queue' whose payload isn't fully written yet. Never
  // touched by write()/the caller thread.
  bool have_header = false;
  FrameHeader current_header;
  /// Scratch buffer taskLoop() reads each frame's payload into - reused
  /// across frames, consumer-side-only, same as have_header above.
  Vector<uint8_t> frame_buf;

  Task task;
  uint32_t task_stack_size = 4096;
  uint8_t task_priority = 2;
  int task_core = -1;

  // Per-frame-type render-time stats - written only from taskLoop(),
  // read via the getters above. Plain atomic (single writer; not for
  // contended updates) - the only shared state left outside 'queue'
  // itself.
  std::atomic<uint32_t> i_frame_count{0};
  std::atomic<uint32_t> p_frame_count{0};
  std::atomic<uint64_t> i_frame_total_ms{0};
  std::atomic<uint64_t> p_frame_total_ms{0};

  // inputFPS()/outputFPS() anchors, set once and never moved - averaging
  // runs "since the very start" up to now. Deliberately different
  // clocks: input_start_ms is clockMs() (see inputFPS()),
  // output_start_ms is always wall-clock millis() (see outputFPS()).
  // input_* is caller-thread-only; output_* is written by taskLoop(),
  // hence atomic.
  uint32_t input_start_ms = 0;
  bool input_start_set = false;
  std::atomic<uint32_t> output_start_ms{0};
  std::atomic<bool> output_start_set{false};

  /// playbackTime() (not millis()) - scheduling against actual audio
  /// progress, not just time passing, is the point of setAudioClock().
  /// Falls back to wall-clock millis() when no audio clock is set.
  uint32_t clockMs() { return p_clock != nullptr ? p_clock->playbackTime() : millis(); }

  /// I/P classification, both for the frameCountI()/frameCountP() stats
  /// and for deciding which frames write()/a resync may ever drop -
  /// delegates to the target's own VideoOutput::isKeyFrame() (see its
  /// comment), since the target is the one that actually knows its
  /// bitstream format.
  bool isKeyFrame(const uint8_t* data, size_t len) {
    return p_target->isKeyFrame(data, len);
  }

  /// How often drainQueueKeepingLastKeyframe() yields (delay(1)) instead
  /// of running straight through - see that method's own comment.
  static constexpr int kDrainYieldEvery = 32;

  /// Drains every whole frame currently sitting in 'queue' - used by
  /// taskLoop()'s resync trigger to abandon a backlog. Consumer-thread-
  /// only.
  ///
  /// Keeps the LAST (freshest) keyframe found along the way instead of
  /// discarding it too: copies it into frame_buf, reports its schedule
  /// time via out_target_ms, returns true. Any keyframe found earlier in
  /// the same backlog is superseded (counted into dropped_i_frame_count).
  /// Every non-keyframe is discarded (counted into dropped_frame_count).
  /// Returns false (frame_buf untouched) if the backlog held no complete
  /// keyframe.
  ///
  /// Stops as soon as the queue doesn't hold a complete next record
  /// (typically a header whose payload hasn't fully arrived - write()
  /// writes header and payload as two separate, non-atomic writeArray()
  /// calls), leaving that partial state for the next call - never
  /// touches queue.reset() (unsafe with a producer concurrently active)
  /// or discards anything mid-record, which would desync the framing
  /// permanently.
  ///
  /// Yields every kDrainYieldEvery frames: this loop is pure memory
  /// copies with no hardware wait to implicitly yield the way normal
  /// rendering does - running it straight through against a large
  /// backlog can starve the FreeRTOS idle task long enough to trip the
  /// watchdog.
  bool drainQueueKeepingLastKeyframe(uint32_t& out_target_ms) {
    bool found_key = false;
    int count = 0;
    while (true) {
      if (!have_header) {
        if ((size_t)queue.available() < sizeof(FrameHeader)) break;
        queue.readArray((uint8_t*)&current_header, sizeof(FrameHeader));
        have_header = true;
      }
      if ((size_t)queue.available() < current_header.size) break;

      if (current_header.is_key) {
        if (found_key) dropped_i_frame_count++;  // superseded by this one
        frame_buf.resize(current_header.size);
        queue.readArray(frame_buf.data(), current_header.size);
        out_target_ms = current_header.target_ms;
        found_key = true;
        queued_i_frame_count--;
      } else {
        uint8_t scratch[256];
        size_t remaining = current_header.size;
        while (remaining > 0) {
          size_t chunk = remaining < sizeof(scratch) ? remaining : sizeof(scratch);
          size_t got = (size_t)queue.readArray(scratch, chunk);
          if (got == 0) break;  // shouldn't happen; avoids ever spinning forever
          remaining -= got;
        }
        dropped_frame_count++;
      }
      have_header = false;
      if (++count % kDrainYieldEvery == 0) delay(1);
    }
    return found_key;
  }

  /// One background-task iteration: pulls one {header, payload} frame out
  /// of 'queue' (across as many calls as needed for the payload to fully
  /// arrive - see have_header), waits until its scheduled presentation
  /// time, then renders it - else just yields. Never called from the
  /// caller's thread.
  void taskLoop() {
    if (!have_header) {
      if ((size_t)queue.available() < sizeof(FrameHeader)) {
        delay(1);
        return;
      }
      queue.readArray((uint8_t*)&current_header, sizeof(FrameHeader));
      have_header = true;
    }
    // The header may already be visible before its payload fully is -
    // write() writes them as two separate writeArray() calls - so just
    // wait for the rest; have_header stays true across calls in the
    // meantime.
    if ((size_t)queue.available() < current_header.size) {
      delay(1);
      return;
    }
    frame_buf.resize(current_header.size);
    queue.readArray(frame_buf.data(), current_header.size);
    uint32_t target_ms = current_header.target_ms;
    bool is_key = current_header.is_key != 0;
    have_header = false;
    // Leaving the queue for rendering (not a discard - see
    // drainQueueKeepingLastKeyframe() for that side of this same counter).
    if (is_key) queued_i_frame_count--;

    // Still waiting for a keyframe after a previous resync - skip
    // straight past any P-frame without considering its schedule,
    // reaching the next keyframe as fast as the queue allows.
    if (awaiting_keyframe && !is_key) {
      dropped_frame_count++;
      frame_count++;
      return;
    }

    if (!start_set) {
      start_ms = clockMs();
      start_set = true;
      LOGI("PacedVideoOutput: playback anchored at %u ms (%s clock)",
           (unsigned)start_ms, p_clock != nullptr ? "external audio" : "wall");
    }
    // scheduling_delay_ms (see setSchedulingDelayMs()) only ever pushes
    // this later, correcting for the audio output's own buffering
    // latency between "accepted by write()" and "actually audible".
    uint32_t scheduled_ms = start_ms + target_ms + scheduling_delay_ms;
    while ((int32_t)(scheduled_ms - clockMs()) > 0) {
      delay(1);
    }
    // Captured right as the wait loop exits, before write()/flush() - a
    // measure of how far behind schedule this frame already was when we
    // started rendering it, separate from processMs (how long rendering
    // itself then took).
    int32_t lateness_ms = (int32_t)(clockMs() - scheduled_ms);
    // Dropping non-keyframes alone can only prevent the backlog from
    // growing, not shrink it - the producer can't deliver frames faster
    // than real time. Past resync_threshold_ms, give up on the skipped
    // content and jump the schedule anchor forward, making *this* frame
    // "on time" - a visible jump instead of an ever-growing lag.
    //
    // Three independent triggers, since they can diverge: lateness_ms
    // only reflects the frame we're about to render - dropped P-frames
    // can keep that low even while newer bytes keep piling up in the
    // queue, since overall render throughput can still trail the arrival
    // rate. queue fill catches that directly, but only once
    // substantially full; queued keyframe count (setMaxQueuedIFrames())
    // catches it earlier still.
    bool lateness_resync =
        resync_threshold_ms > 0 && lateness_ms > (int32_t)resync_threshold_ms;
    bool queue_resync =
        resync_queue_fill_fraction > 0 && queue.size() > 0 &&
        (float)queue.available() >=
            resync_queue_fill_fraction * (float)queue.size();
    bool iframe_resync = max_queued_i_frames > 0 &&
                          queued_i_frame_count.load() >= max_queued_i_frames;
    if (lateness_resync || queue_resync || iframe_resync) {
      // Keep the freshest keyframe found in the backlog instead of
      // discarding it too (see drainQueueKeepingLastKeyframe()).
      uint32_t new_target_ms = target_ms;
      bool found_fresher_key = drainQueueKeepingLastKeyframe(new_target_ms);
      LOGW(
          "PacedVideoOutput: resyncing (%s) - %s instead of trying to "
          "catch up",
          lateness_resync
              ? "lateness"
              : (queue_resync ? "queue fill" : "I-frame backlog"),
          found_fresher_key ? "jumping to the newest available keyframe"
                             : "jumping the schedule forward");
      if (found_fresher_key) {
        // Supersede whatever was already dequeued.
        if (is_key) {
          dropped_i_frame_count++;
        } else {
          dropped_frame_count++;
        }
        target_ms = new_target_ms;
        is_key = true;
        awaiting_keyframe = false;
      } else {
        // A resync can leave the decoder's reference state unreliable,
        // so require a real (self-contained) keyframe before resuming -
        // discard any P-frame in between (see the check below) - unless
        // the frame we already have is one.
        awaiting_keyframe = true;
      }
      // Recomputed for whichever frame we're actually about to render.
      start_ms = clockMs() - target_ms - scheduling_delay_ms;
      scheduled_ms = start_ms + target_ms + scheduling_delay_ms;  // == clockMs() now
      lateness_ms = 0;
    }
    if (awaiting_keyframe) {
      if (!is_key) {
        dropped_frame_count++;
        frame_count++;
        render_lateness_ms = 0;
        return;
      }
      awaiting_keyframe = false;
    }
    // Published for write()'s proactive drop check (see
    // setCatchUpThresholdFrames()) before doing the possibly-slow render
    // call, so a caller blocked in write() sees it as soon as possible
    // rather than only after this frame finishes too.
    render_lateness_ms = lateness_ms;
    uint32_t processStart = millis();
    p_target->write(frame_buf.data(), frame_buf.size());
    p_target->flush();
    uint32_t processMs = millis() - processStart;
    // See VideoOutput::hadOutput() - true for every synchronous decoder
    // (the common case), but a decoder like MPGDecoder can legitimately
    // do real decode work here without a picture actually reaching the
    // screen yet (held back for B-picture display-order reordering), or
    // emit an earlier held picture instead. Only count/time this call as
    // a rendered frame when a picture was actually produced - otherwise
    // outputFPS()/frameCountI()/frameCountP()/avgFrameMs() would
    // overcount calls that did no real rendering work.
    if (p_target->hadOutput()) {
      if (!output_start_set) {
        output_start_ms = millis();
        output_start_set = true;
      }
      if (is_key) {
        i_frame_count++;
        i_frame_total_ms += processMs;
      } else {
        p_frame_count++;
        p_frame_total_ms += processMs;
      }
      LOGD(
          "PacedVideoOutput: rendered %s frame - scheduled=%u actual=%u "
          "(%d ms late), process=%u ms",
          is_key ? "I" : "P", (unsigned)scheduled_ms, (unsigned)clockMs(),
          (int)lateness_ms, (unsigned)processMs);
    } else {
      LOGD(
          "PacedVideoOutput: decoded %s frame, no picture emitted yet "
          "(scheduled=%u actual=%u, %d ms late), process=%u ms",
          is_key ? "I" : "P", (unsigned)scheduled_ms, (unsigned)clockMs(),
          (int)lateness_ms, (unsigned)processMs);
    }
    if (frame_period_ms > 0 && processMs > (uint32_t)frame_period_ms) {
      LOGW(
          "PacedVideoOutput: %s frame took %u ms to render - longer than "
          "the %.1f ms frame period, falling behind",
          is_key ? "I" : "P", (unsigned)processMs, frame_period_ms);
    }
  }
};

}  // namespace audio_tools
