// Linux Task implementation using std::thread to mimic FreeRTOS Task API
#pragma once
#ifdef USE_CPP_TASK

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace audio_tools {

/**
 * @brief Task abstraction for Linux mapped to a std::thread.
 * suspend()/resume() simulated with condition_variable.
 */
class Task {
 public:
  Task(const char *name, int stackSize, int priority = 1, int core = -1) {
    (void)name;
    (void)stackSize;
    (void)priority;
    (void)core;  // parameters unused
  }
  Task() = default;
  ~Task() { remove(); }

  bool create(const char *name, int stackSize, int priority = 1,
              int core = -1) {
    (void)name;
    (void)stackSize;
    (void)priority;
    (void)core;  // parameters unused
    return true;
  }

  bool begin(std::function<void()> process) {
    if (running_thread.joinable()) return false;  // already running
    loop_code = process;
    terminate_flag = false;
    // start suspended similar to FreeRTOS create+suspend pattern
    paused = false;  
    running_thread = std::thread([this] { this->thread_loop(); });
    return true;
  }

  void end() { remove(); }

  void remove() {
    terminate_flag = true;
    resume();  // wake if paused
    if (running_thread.joinable()) {
      if (std::this_thread::get_id() == running_thread.get_id()) {
        // Avoid deadlock: cannot join the current thread; detach instead
        running_thread.detach();
      } else {
        running_thread.join();
      }
    }
  }

  void suspend() {
    std::lock_guard<std::mutex> lk(mtx);
    paused = true;
  }

  void resume() {
    {
      std::lock_guard<std::mutex> lk(mtx);
      paused = false;
    }
    cv.notify_all();
  }

  // API compatibility – return thread id reference surrogate
  std::thread::id &getTaskHandle() { return thread_id; }

  void setReference(void *r) { ref = r; }
  void *getReference() { return ref; }

 protected:
  std::thread running_thread;
  std::thread::id thread_id{};
  std::function<void()> loop_code = nop;
  void *ref = nullptr;
  std::atomic<bool> terminate_flag{false};
  std::mutex mtx;
  std::condition_variable cv;
  bool paused = false;

  static void nop() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  void thread_loop() {
    thread_id = std::this_thread::get_id();
    while (!terminate_flag.load()) {
      // wait while paused
      {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [this] { return !paused || terminate_flag.load(); });
      }
      if (terminate_flag.load()) break;
      if (loop_code)
        loop_code();
      else
        nop();
    }
  }
};

}  // namespace audio_tools

#endif  // __linux__
