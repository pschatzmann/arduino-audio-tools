#pragma once

#include "AudioTools/CoreAudio/AudioPlayer.h"

namespace audio_tools {
/// Control AudioPlayer command types processed in copy()

enum class AudioPlayerCommandType {
  Begin,
  End,
  Next,
  SetIndex,
  SetPath,
  SetVolume,
  SetMuted,
  SetActive
};

struct AudioPlayerCommand {
  AudioPlayerCommandType type;
  int index = 0;         // begin/setIndex
  bool isActive = true;  // begin/setActive
  int offset = 1;        // next
  float volume = 0.0f;   // setVolume
  bool muted = false;    // setMuted
};

/**
 * @class AudioPlayerThreadSafe
 * @brief Lock-free asynchronous control wrapper for AudioPlayer using a command
 * queue.
 *
 * Purpose
 * Provides a minimal, thread-safe control surface (begin, end, next, setIndex,
 * setPath, setVolume, setMuted, setActive) by enqueuing commands from any task
 * and applying them inside copy() in the audio/render thread. This serializes
 * all state changes without a mutex.
 *
 * Contract
 * - Input: Reference to an existing AudioPlayer instance + queue capacity.
 * - API calls: enqueue a Command (non-blocking if underlying queue is
 * configured with zero read/write wait). No direct state mutation happens in
 * the caller's context.
 * - Execution: copy()/copy(bytes) drains the queue first (processCommands())
 * then performs audio transfer via AudioPlayer::copy(). Order is preserved
 * (FIFO).
 * - Errors: enqueue() returns false if the queue is full (command dropped).
 * Caller may retry later. No blocking is performed by this wrapper. Dequeue in
 * processCommands() assumes the queue's read wait is non-blocking (e.g.
 * QueueRTOS.setReadMaxWait(0)).
 * - Path lifetime: setPath(const char*) stores the pointer; caller must keep
 * the memory valid until the command is consumed. If the path buffer is
 * ephemeral, allocate/copy it externally or extend the Command to own storage
 * (future enhancement).
 *
 * Thread-safety model
 * - All public control methods are producer-only; they never touch the
 * AudioPlayer directly.
 * - The audio thread (calling copy()) is the single consumer applying changes,
 * preventing races.
 * - No mutexes or locks are used; correctness relies on queue's internal
 * synchronization.
 *
 * Callback / reentrancy guidance
 * - Avoid calling wrapper control methods from callbacks invoked by copy()
 * (e.g. EOF callbacks) to prevent immediate feedback loops; schedule such
 * actions from another task.
 *
 * Template parameter
 * - QueueT: a queue class template <class T> providing: constructor(int
 * size,...), bool enqueue(T&), bool dequeue(T&). Example: QueueRTOS.
 *
 * @tparam QueueT Queue class template taking a single type parameter (the
 * command type).
 * @ingroup concurrency
 */
template <template <class> class QueueT>
class AudioPlayerThreadSafe {
 public:
  /**
   * @brief Construct an async-control wrapper around an AudioPlayer.
   * @param p Underlying AudioPlayer to protect
   * @param queueSize Capacity of the internal command queue
   */
  AudioPlayerThreadSafe(AudioPlayer& p, QueueT<AudioPlayerCommand>& queue)
      : player(p), queue(queue) {}

  // Control API: enqueue only; applied in copy()
  bool begin(int index = 0, bool isActive = true) {
    AudioPlayerCommand c{AudioPlayerCommandType::Begin};
    c.index = index;
    c.isActive = isActive;
    return enqueue(c);
  }

  void end() {
    AudioPlayerCommand c{AudioPlayerCommandType::End};
    enqueue(c);
  }

  bool next(int offset = 1) {
    AudioPlayerCommand c{AudioPlayerCommandType::Next};
    c.offset = offset;
    return enqueue(c);
  }

  bool setIndex(int idx) {
    AudioPlayerCommand c{AudioPlayerCommandType::SetIndex};
    c.index = idx;
    return enqueue(c);
  }

  bool setPath(const char* path) {
    AudioPlayerCommand c{AudioPlayerCommandType::SetPath};
    this->path = path;
    return enqueue(c);
  }

  size_t copy() {
    if (queue.size() > 0) processCommands();
    return player.copy();
  }

  size_t copy(size_t bytes) {
    if (queue.size() > 0) processCommands();
    return player.copy(bytes);
  }

  void setActive(bool active) {
    AudioPlayerCommand c{AudioPlayerCommandType::SetActive};
    c.isActive = active;
    enqueue(c);
  }

  bool setVolume(float v) {
    AudioPlayerCommand c{AudioPlayerCommandType::SetVolume};
    c.volume = v;
    return enqueue(c);
  }

  bool setMuted(bool muted) {
    AudioPlayerCommand c{AudioPlayerCommandType::SetMuted};
    c.muted = muted;
    return enqueue(c);
  }

 private:
  AudioPlayer& player;
  // Internal command queue
  QueueT<AudioPlayerCommand>& queue;
  Str path;

  // Drain command queue and apply to underlying player
  void processCommands() {
    AudioPlayerCommand cmd;
    // Attempt non-blocking dequeue loop; requires queue configured non-blocking
    while (dequeue(cmd)) {
      switch (cmd.type) {
        case AudioPlayerCommandType::Begin:
          player.begin(cmd.index, cmd.isActive);
          break;
        case AudioPlayerCommandType::End:
          player.end();
          break;
        case AudioPlayerCommandType::Next:
          player.next(cmd.offset);
          break;
        case AudioPlayerCommandType::SetIndex:
          player.setIndex(cmd.index);
          break;
        case AudioPlayerCommandType::SetPath:
          player.setPath(path.c_str());
          break;
        case AudioPlayerCommandType::SetVolume:
          player.setVolume(cmd.volume);
          break;
        case AudioPlayerCommandType::SetMuted:
          player.setMuted(cmd.muted);
          break;
        case AudioPlayerCommandType::SetActive:
          player.setActive(cmd.isActive);
          break;
      }
      if (queue.size() == 0) break;
    }
  }

  // Queue facade wrappers to allow both internal/external queues
  bool enqueue(AudioPlayerCommand& c) { return queue.enqueue(c); }

  bool dequeue(AudioPlayerCommand& c) { return queue.dequeue(c); }
};

}  // namespace audio_tools
