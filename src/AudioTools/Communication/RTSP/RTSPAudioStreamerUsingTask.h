/*
 * Author: Phil Schatzmann
 *
 * Based on Micro-RTSP library:
 * https://github.com/geeksville/Micro-RTSP
 * https://github.com/Tomp0801/Micro-RTSP-Audio
 */
#pragma once
#include "RTSPAudioStreamer.h"

namespace audio_tools {

/**
 * @brief RTSPAudioStreamerUsingTask - Task-driven RTP Audio Streaming Engine
 *
 * The RTSPAudioStreamerUsingTask class extends RTSPAudioStreamerBase with
 * AudioTools Task-driven streaming functionality. Instead of using hardware
 * timers, this class creates a dedicated task that continuously calls the
 * timer callback at the appropriate intervals. This approach provides:
 *
 * - All base class functionality (audio source, UDP transport, RTP packets)
 * - Task-based periodic streaming using AudioTools Task class
 * - More predictable scheduling compared to hardware timers
 * - Better resource control and priority management
 * - Reduced timer resource usage
 * - Optional throttling for timing control
 *
 * The throttling feature is particularly important when working with audio
 * sources that can provide data faster than the defined sampling rate, such
 * as file readers, buffer sources, or fast generators. Without throttling,
 * these sources would flood the network with packets faster than real-time
 * playback.
 *
 * @ingroup rtsp
 * @author Phil Schatzmann
 */
template <typename Platform>
class RTSPAudioStreamerUsingTask : public RTSPAudioStreamerBase<Platform> {
 public:
  RTSPAudioStreamerUsingTask(bool throttled = true)
      : RTSPAudioStreamerBase<Platform>() {
    LOGD("Creating RTSP Audio streamer with task");
    m_taskRunning = false;
    m_throttled = throttled;
  }

  RTSPAudioStreamerUsingTask(IAudioSource &source, bool throttled = true)
      : RTSPAudioStreamerUsingTask(throttled) {
    this->setAudioSource(&source);
  }

  virtual ~RTSPAudioStreamerUsingTask() { stop(); }

  void setTaskParameters(uint32_t stackSize, uint8_t priority, int core = -1) {
    if (!m_taskRunning) {
      m_taskStackSize = stackSize;
      m_taskPriority = priority;
      m_taskCore = core;
      LOGI("Task parameters set: stack=%d bytes, priority=%d, core=%d",
           stackSize, priority, core);
    } else {
      LOGW("Cannot change task parameters while streaming is active");
    }
  }

  void start() override {
    LOGI("Starting RTP Stream with task");
    RTSPAudioStreamerBase<Platform>::start();
    if (this->m_audioSource != nullptr && !m_taskRunning) {
      m_taskRunning = true;
      if (!m_streamingTask.create("RTSPStreaming", m_taskStackSize,
                                  m_taskPriority, m_taskCore)) {
        LOGE("Failed to create streaming task");
        m_taskRunning = false;
        return;
      }
      m_streamingTask.setReference(this);
      m_send_counter = 0;
      m_last_throttle_us = micros();
      if (m_streamingTask.begin([this]() { this->streamingTaskLoop(); })) {
        LOGI("Streaming task started successfully");
        LOGI("Task: stack=%d bytes, priority=%d, core=%d, period=%d us",
             m_taskStackSize, m_taskPriority, m_taskCore,
             this->m_timer_period_us);
#ifdef ESP32
        LOGI("Free heap size: %i KB", esp_get_free_heap_size() / 1000);
#endif
      } else {
        LOGE("Failed to start streaming task");
        m_taskRunning = false;
      }
    }
  }

  void stop() override {
    LOGI("Stopping RTP Stream with task");
    if (m_taskRunning) {
      m_taskRunning = false;
      m_streamingTask.end();
      delay(50);
    }
    RTSPAudioStreamerBase<Platform>::stop();
    LOGI("RTP Stream with task stopped - ready for restart");
  }

  bool isTaskRunning() const { return m_taskRunning; }
  void setThrottled(bool isThrottled) { m_throttled = isThrottled; }
  void setFixedDelayMs(uint32_t delayUs) { this->m_fixed_delay_ms = delayUs; m_throttled = false; }
  void setThrottleInterval(uint32_t interval) { m_throttle_interval = interval; }

 protected:
  audio_tools::Task m_streamingTask;
  volatile bool m_taskRunning;
  uint32_t m_taskStackSize = 8192;
  uint8_t m_taskPriority = 5;
  int m_taskCore = -1;
  bool m_throttled = true;
  uint16_t m_fixed_delay_ms = 1;
  uint32_t m_throttle_interval = 50;
  uint32_t m_send_counter = 0;
  unsigned long m_last_throttle_us = 0;

  void streamingTaskLoop() {
    LOGD("Streaming task loop iteration");
    auto iterationStartUs = micros();
    RTSPAudioStreamerBase<Platform>::timerCallback(this);
    applyThrottling(iterationStartUs);
  }

  inline void applyThrottling(unsigned long iterationStartUs) {
    ++m_send_counter;
    delay(m_fixed_delay_ms);
    if (m_throttled && m_throttle_interval > 0) {
      if (this->checkTimerPeriodChange()) {
        LOGI("Timer period updated; resetting throttle window to %u us",
             (unsigned)this->m_timer_period_us);
        m_send_counter = 0;
        m_last_throttle_us = iterationStartUs;
        return;
      }
      if (m_send_counter >= m_throttle_interval) {
        uint64_t expectedUs = (uint64_t)m_throttle_interval * (uint64_t)this->getTimerPeriodUs();
        unsigned long nowUs = micros();
        uint64_t actualUs = (uint64_t)(nowUs - m_last_throttle_us);
        if (actualUs < expectedUs) {
          uint32_t remainingUs = (uint32_t)(expectedUs - actualUs);
          if (remainingUs >= 1000) delay(remainingUs / 1000);
          uint32_t remUs = remainingUs % 1000;
          if (remUs > 0) delayMicroseconds(remUs);
        }
        m_send_counter = 0;
        m_last_throttle_us = micros();
      }
    }
  }
};

} // namespace audio_tools
