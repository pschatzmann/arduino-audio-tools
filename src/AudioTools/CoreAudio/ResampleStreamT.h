#pragma once
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/Buffers.h"

namespace audio_tools {

/**
 * @brief Base class for resampling algorithms using a buffer of float values.
 *
 * Call getValue() to get the next interpolated value until it returns false.
 * Then provide a new value with addValue() to continue.
 *
 * Derived classes must implement the value() and vectorSize() methods for
 * interpolation. The calculation depends on n values.
 * For example, a linear interpolation only needs the last two values, while a
 * cubic B-spline interpolation requires the last four values.
 * The buffer is always filled with the required number of values, so the
 * derived class can assume that it has access to all required values for
 * interpolation.
 *
 * The buffer is initilaized with 0 values and it is re-initialized when
 * setStepSize() is called with a different size.
 */
struct BaseInterpolator {
  /**
   * @brief Constructor. Initializes the buffer with four zeros.
   */
  bool begin() {
    // make sure the buffer has enough space for entries
    int size = vectorSize();
    values.resize(size);
    return true;
  }

  /**
   * @brief Adds a new value to the buffer, discarding the oldest.
   * @param value The new sample value to add.
   */
  void addValue(float value) {
    // keep buffer at the minimum size
    if (values.available() == vectorSize()) {
      step -= 1.0f;
      values.clearArray(1);
    }
    // add the new value
    values.write(value);
  }
  /**
   * @brief Gets the next interpolated value.
   * @param newValue Reference to store the interpolated value.
   * @return True if a value was produced, false if more input is needed.
   */

  bool getValue(float& newValue) {
    // limit the step between 0 and 1
    if (step >= 1.0f) return false;
    // make sure we have enough values in the buffer
    if (values.available() < vectorSize()) return false;

    // calculate the interpolated value
    newValue = valueExt(values.address(), step);

    // determine next step
    step += step_size;

    return true;
  }

  float valueExt(float* y, float xf) {
    return value(y, xf);
  }

  /// Returns true if there are more values to read
  operator bool() const {
    int step_copy = step;
    if (step_copy >= 1.0) return false;
    return true;
  }

  /**
   * @brief Sets the step size for interpolation.
   * @param step The new step size.
   */

  void setStepSize(float step) {
    if (step_size == step) return;
    step_size = step;
    // clear the buffer to start fresh
    values.clear();
  }

 protected:
  SingleBuffer<float>
      values;  ///< Buffer holding the last 4 values for interpolation.
  float step_size = 1.0f;  ///< Step size for resampling, default is 1.0.
  float step = 0;

  virtual int vectorSize() = 0;

  /**
   * @brief Interpolation function to be implemented by derived classes.
   * @param y Pointer to the buffer of 4 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  virtual float value(float* y, float xf) = 0;
};

/**
 * @brief Linear interpolation between y[0] and y[1].
 *
 * Valid for xf in [0,1].
 */
struct LinearInterpolator : public BaseInterpolator {
  int vectorSize() {
    return 2;
  }  ///< Minimum number of values required for interpolation
  /**
   * @brief Computes the linear interpolation.
   * @param y Pointer to at least 2 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  float value(float* y, float xf) {    
    if (xf == 0.0f) return y[0];
    if (xf == 1.0f) return y[1];

    int x = xf;
    float dx = xf - x;
    return y[x] + dx * (y[x + 1] - y[x]);
  }
};

/**
 * @brief Cubic B-spline interpolation using y[0]..y[3].
 *
 * Valid for xf in [0,3].
 */
struct BSplineInterpolator : public BaseInterpolator {
  int vectorSize() {
    return 4;
  }  ///< Minimum number of values required for interpolation
  /**
   * @brief Computes the cubic B-spline interpolation.
   * @param y Pointer to at least 4 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  float value(float* y, float xf) {
    int x = xf;
    float dx = xf - x;
    // Shift all indices up by 1: y[x-1] -> y[x], y[x] -> y[x+1], etc.
    float ym1py1 = y[x] + y[x + 2];
    float c0 = 1.0f / 6.0f * ym1py1 + 2.0f / 3.0f * y[x + 1];
    float c1 = 1.0f / 2.0f * (y[x + 2] - y[x]);
    float c2 = 1.0f / 2.0f * ym1py1 - y[x + 1];
    float c3 =
        1.0f / 2.0f * (y[x + 1] - y[x + 2]) + 1.0f / 6.0f * (y[x + 3] - y[x]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

/**
 * @brief Cubic Lagrange interpolation using y[0]..y[3].
 *
 * Valid for xf in [0,3].
 */
struct LagrangeInterpolator : public BaseInterpolator {
  int vectorSize() {
    return 4;
  }  ///< Minimum number of values required for interpolation
  /**
   * @brief Computes the cubic Lagrange interpolation.
   * @param y Pointer to at least 4 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  float value(float* y, float xf) {
    int x = xf;
    float dx = xf - x;
    // Shift all indices up by 1
    float c0 = y[x + 1];
    float c1 = y[x + 2] - 1.0f / 3.0f * y[x] - 1.0f / 2.0f * y[x + 1] -
               1.0f / 6.0f * y[x + 3];
    float c2 = 1.0f / 2.0f * (y[x] + y[x + 2]) - y[x + 1];
    float c3 =
        1.0f / 6.0f * (y[x + 3] - y[x]) + 1.0f / 2.0f * (y[x + 1] - y[x + 2]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

/**
 * @brief Cubic Hermite interpolation using y[0]..y[3].
 *
 * Valid for xf in [0,3].
 */
struct HermiteInterpolator : public BaseInterpolator {
  int vectorSize() {
    return 4;
  }  ///< Minimum number of values required for interpolation
  /**
   * @brief Computes the cubic Hermite interpolation.
   * @param y Pointer to at least 4 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  float value(float* y, float xf) {
    int x = xf;
    float dx = xf - x;
    // Shift all indices up by 1
    float c0 = y[x + 1];
    float c1 = 1.0f / 2.0f * (y[x + 2] - y[x]);
    float c2 = y[x] - 5.0f / 2.0f * y[x + 1] + 2.0f * y[x + 2] -
               1.0f / 2.0f * y[x + 3];
    float c3 =
        1.0f / 2.0f * (y[x + 3] - y[x]) + 3.0f / 2.0f * (y[x + 1] - y[x + 2]);
    return ((c3 * dx + c2) * dx + c1) * dx + c0;
  }
};

/**
 * @brief Parabolic interpolation using y[0]..y[3].
 *
 * Valid for xf in [0,3].
 */
struct ParabolicInterpolator : public BaseInterpolator {
  int vectorSize() {
    return 4;
  }  ///< Minimum number of values required for interpolation
  /**
   * @brief Computes the parabolic interpolation.
   * @param y Pointer to at least 4 values.
   * @param xf The fractional index for interpolation.
   * @return Interpolated value.
   */
  float value(float* y, float xf) {
    int x = xf;
    float dx = xf - x;
    // Shift all indices up by 1
    float y1mym1 = y[x + 2] - y[x];
    float c0 = 1.0f / 2.0f * y[x + 1] + 1.0f / 4.0f * (y[x] + y[x + 2]);
    float c1 = 1.0f / 2.0f * y1mym1;
    float c2 = 1.0f / 4.0f * (y[x + 3] - y[x + 1] - y1mym1);
    return (c2 * dx + c1) * dx + c0;
  }
};

/**
 * @brief Multi-channel resampler that applies a BaseInterpolator-derived algorithm
 * to each channel.
 *
 * @tparam TInterpolator The resampler type (must derive from BaseInterpolator).
 */
template <class TInterpolator>
class MultiChannelResampler {
 public:
  void setChannels(int channels) {
    if (_channels == channels) return;

    _resamplers.resize(channels);
    // create new resamplers
    _channels = channels;
    for (int i = 0; i < _channels; ++i) {
      _resamplers[i].begin();
    }
  }

  /**
   * @brief Sets the step size for all channels.
   * @param step The new step size.
   */
  void setStepSize(float step) {
    for (int i = 0; i < _channels; ++i) {
      _resamplers[i].setStepSize(step);
    }
  }

  /**
   * @brief Adds a new value for each channel.
   * @param values Array of input values, one per channel.
   */
  void addValues(const float* values) {
    for (int i = 0; i < _channels; ++i) {
      _resamplers[i].addValue(values[i]);
    }
  }

  void addValue(float value, int channel) {
    if (channel < 0 || channel >= _channels) {
      LOGE("Invalid channel index: %d", channel);
      return;
    }
    _resamplers[channel].addValue(value);
  }

  /**
   * @brief Gets the next interpolated value for each channel.
   * @param out Array to store the interpolated values, one per channel.
   * @return True if values were produced, false if more input is needed.
   */
  bool getValues(float* out) {
    bool ok = true;
    for (int i = 0; i < _channels; ++i) {
      ok &= _resamplers[i].getValue(out[i]);
    }
    return ok;
  }

  operator bool() const {
    if (_channels <= 0) return false;
    return _resamplers[0] && static_cast<bool>(_resamplers[0]);
  }

  /**
   * @brief Returns the number of channels.
   */
  int channels() const { return _channels; }

 protected:
  int _channels = 0;
  Vector<TInterpolator> _resamplers;
};

/**
 * @brief A Stream implementation for resamping using a specified interpolation
 * algorithm.
 * @tparam TInterpolator The resampler type (must derive from BaseInterpolator).
 */
template <class TInterpolator>
class ResampleStreamT : public ReformatBaseStream {
 public:
  /**
   * @brief Default constructor.
   */
  ResampleStreamT() = default;
  /**
   * @brief Constructor for output to a Print interface.
   * @param out The Print interface to write resampled data to.
   */

  ResampleStreamT(Print& out) { setOutput(out); }
  /**
   * @brief Constructor for output to an AudioOutput interface.
   * @param out The AudioOutput interface to write resampled data to.
   */

  ResampleStreamT(AudioOutput& out) {
    setAudioInfo(out.audioInfo());
    setOutput(out);
  }
  /**
   * @brief Constructor for input/output via a Stream interface.
   * @param io The Stream interface to read/write data.
   */
  ResampleStreamT(Stream& io) { setStream(io); }
  /**
   * @brief Constructor for input/output via an AudioStream interface.
   * @param io The AudioStream interface to read/write data.
   */
  ResampleStreamT(AudioStream& io) {
    setAudioInfo(io.audioInfo());
    setStream(io);
  }
  /**
   * @brief Initializes the resampler with the given configuration.
   * @param cfg The resampling configuration.
   * @return True if initialization was successful.
   */

  bool begin(ResampleConfig cfg) {
    this->cfg = cfg;
    setAudioInfo(cfg);
    return begin();
    ;
  }
  /**
   * @brief Initializes the resampler with audio info and a step size.
   * @param info The audio format information.
   * @param step The resampling step size.
   * @return True if initialization was successful.
   */

  bool begin(AudioInfo info, float step) {
    cfg.copyFrom(info);
    cfg.step_size = step;
    return begin();
  }

  bool begin() {
    setupReader();
    if (cfg.step_size) {
      setStepSize(cfg.step_size);
    } else if (cfg.to_sample_rate > 0) {
      // calculate step size from sample rate
      cfg.step_size = static_cast<float>(cfg.sample_rate) /
                      static_cast<float>(cfg.to_sample_rate);
      setStepSize(cfg.step_size);
    } else {
      cfg.step_size = 1.0f;
      setStepSize(1.0f);
    }
    return true;
  }

  /**
   * @brief Sets the resampling step size for all channels.
   * @param step The new step size.
   */

  void setStepSize(float step) {
    cfg.step_size = step;
    _resampler.setStepSize(step);
  }
  /**
   * @brief Gets the current resampling step size.
   * @return The step size.
   */

  float getStepSize() const { return cfg.step_size; }

  /**
   * @brief Returns the output audio information, adjusting the sample rate
   * according to the step size.
   * @return The output AudioInfo.
   */
  AudioInfo audioInfoOut() override {
    AudioInfo out = audioInfo();
    if (cfg.to_sample_rate > 0) {
      out.sample_rate = cfg.to_sample_rate;
    } else if (cfg.step_size != 1.0f) {
      out.sample_rate = out.sample_rate / cfg.step_size;
    }
    return out;
  }

  /**
   * @brief Write interleaved samples to the stream
   */
  size_t write(const uint8_t* data, size_t len) override {
    LOGD("ResampleStreamT::write: %d", (int)len);
    // addNotifyOnFirstWrite();
    size_t written = 0;
    switch (info.bits_per_sample) {
      case 16:
         writeT<int16_t>(p_print, data, len, written);
         break;
      case 24:
         writeT<int24_t>(p_print, data, len, written);
         break;
      case 32:
         writeT<int32_t>(p_print, data, len, written);
         break;
      default:
        TRACEE();
    }
    return len;
  }

  /**
   * @brief Sets the audio format information and updates the channel count.
   * @param newInfo The new audio format information.
   */

  void setAudioInfo(AudioInfo newInfo) override {
    AudioStream::setAudioInfo(newInfo);
    _resampler.setChannels(newInfo.channels);
    cfg.copyFrom(newInfo);
    // if target sample rate is set, calculate step size
    if (cfg.to_sample_rate > 0) {
      // calculate step size from sample rate
      cfg.step_size = static_cast<float>(cfg.sample_rate) /
                      static_cast<float>(cfg.to_sample_rate);
      setStepSize(cfg.step_size);
    } 
  }

  float getByteFactor() override { return 1.0f / cfg.step_size; }

  ResampleConfig defaultConfig() {
    ResampleConfig result;
    return result;
  }

 protected:
  MultiChannelResampler<TInterpolator> _resampler;
  ResampleConfig cfg;

  /// Writes the buffer to defined output after resampling
  template <typename T>
  size_t writeT(Print* p_out, const uint8_t* buffer, size_t bytes,
                size_t& written) {
    if (p_out == nullptr) return 0;

    // If no resampling is needed, write directly
    if (cfg.step_size == 1.0f) {
      return p_out->write(buffer, bytes);
    }

    // resample the data
    int channels = audioInfo().channels;
    T* data = (T*)buffer;
    float frame[channels];
    size_t frames = bytes / (sizeof(T) * channels);
    size_t frames_written = 0;
    for (size_t i = 0; i < frames; ++i) {
      // fill frame (of floats) with values from data
      for (int ch = 0; ch < channels; ++ch) {
        frame[ch] = static_cast<float>(data[i * channels + ch]);
      }

      // Add values to resampler
      _resampler.addValues(frame);

      // Get resampled values
      float result[channels];
      while (_resampler.getValues(result)) {
        // Convert float to correct output type
        T resultT[channels];
        for (int ch = 0; ch < channels; ++ch) {
          resultT[ch] = NumberConverter::clipT<T>(result[ch]);
        }

        // Write the frame
        size_t to_write = sizeof(T) * channels;
        size_t written = p_out->write((const uint8_t*)resultT, to_write);
        if (written != to_write) {
          LOGE("write error %zu -> %zu", to_write, written);
        }
        frames_written++;
      }
    }

    return frames_written * sizeof(T) * channels;
  }
};

}  // namespace audio_tools