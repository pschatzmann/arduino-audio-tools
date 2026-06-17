#pragma once

namespace audio_tools {

/**
 * @brief Interface for a task that can be started and stopped.
 * @ingroup concurrency
 * @author Phil Schatzmann
 */
class ITask {
 public:
  virtual ~ITask() = default;
  virtual bool begin(std::function<void()> process) = 0;
  virtual void end() = 0;
};

}  // namespace audio_tools