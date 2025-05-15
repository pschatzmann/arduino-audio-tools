#pragma once

#include "AudioTools/AudioLibs/PIDController.h"
#include "AudioTools/CoreAudio/AudioBasic/MovingAverage.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

namespace audio_tools {
/**
 * @brief An Audio Stream backed by a buffer (queue) which tries to correct
 * jitter and automatically adjusts for the slightly different clock rates
 * between an audio source and audio target. Use separate tasks to write and
 * read the data. Also make sure that you protect the access with a mutex or
 * provide a thread save buffer!
 * 
 * The resamping step size is calculated with the help of a PID controller.
 * 
 * @ingroup buffers
 * @author Phil Schatzmann
 */
class DynamicResamplingQueueStream : public AudioStream {
 public:
  DynamicResamplingQueueStream(BaseBuffer<uint8_t>& buffer,
                               float stepRangePercent = 0.05) {
    p_buffer = &buffer;
    setStepRangePercent(stepRangePercent);
    addNotifyAudioChange(resample_stream);
  }

  bool begin() {
    if (p_buffer == nullptr) return false;
    queue_stream.setBuffer(*p_buffer);
    queue_stream.begin();
    resample_stream.setAudioInfo(audioInfo());
    resample_stream.setStream(queue_stream);
    resample_stream.begin(audioInfo());
    float from_step = 1.0 - resample_range;
    float to_step = 1.0 + resample_range;
    return pid.begin(1.0, from_step, to_step, p, i, d);
  }

  /// Fill the buffer
  size_t write(const uint8_t* data, size_t len) override {
    if (p_buffer == 0) return 0;
    return p_buffer->writeArray(data, len);
  }

  void end() {
    queue_stream.end();
    resample_stream.end();
  }

  /// Read resampled data from the buffer
  size_t readBytes(uint8_t* data, size_t len) override {
    if (p_buffer->available() == 0) return 0;

    // calculate new resampling step size
    moving_average_level_percent.add(p_buffer->levelPercent());
    step_size = pid.calculate(50.0, moving_average_level_percent.average());

    // log step size every 10th read
    if (read_count++ % 10 == 0) {
      LOGI("step_size: %f", step_size);
    }

    // return resampled result
    resample_stream.setStepSize(step_size);
    return resample_stream.readBytes(data, len);
  }

  /// Defines the number of historic %fill levels that will be used to calculate
  /// the moving avg
  void setMovingAvgCount(int size) {
    moving_average_level_percent.setSize(size);
  }

  /// e.g. a value of 0.0005 means that we allow to resample for 44100 by
  /// +- 22.05 from 44077.95 to 44122.05
  void setStepRangePercent(float rangePercent) {
    resample_range = rangePercent / 100.0;
  }

  /// Define the PID parameters
  void setPIDParameters(float p_value, float i_value, float d_value) {
    p = p_value;
    i = i_value;
    d = d_value;
  }

 protected:
  PIDController pid;  // p=0.005, i=0.00005, d=0.0001
  QueueStream<uint8_t> queue_stream;
  BaseBuffer<uint8_t>* p_buffer = nullptr;
  MovingAverage<float> moving_average_level_percent{50};
  ResampleStream resample_stream;
  float step_size = 1.0;
  float resample_range = 0;
  float p = 0.005;
  float i = 0.00005;
  float d = 0.0001;
  uint32_t read_count = 0;
};

}  // namespace audio_tools
