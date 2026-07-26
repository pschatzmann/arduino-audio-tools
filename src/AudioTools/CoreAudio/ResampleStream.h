#pragma once

#include "AudioTools/CoreAudio/AudioIO.h"

#if USE_PRINT_FLUSH
#  define PRINT_FLUSH_OVERRIDE override
#else
#  define PRINT_FLUSH_OVERRIDE
#endif

// Forced processing at 1.0f has been disabled in this local build.

namespace audio_tools {

/**
 * @brief Optional Configuration object. The critical information is the
 * channels and the step_size. All other information is not used.
 *
 */
struct ResampleConfig : public AudioInfo {
  float step_size = 1.0f;
  /// Optional fixed target sample rate
  int to_sample_rate = 0;
  int buffer_size = DEFAULT_BUFFER_SIZE;
};

/**
 * @brief Dynamic Resampling. We can use a variable factor to speed up or slow
 * down the playback.
 * @author Phil Schatzmann
 * @ingroup transform
 * @copyright GPLv3
 * @tparam T
 */
class ResampleStream : public ReformatBaseStream {
 public:
  ResampleStream() = default;

  /// Support for resampling via write.
  ResampleStream(Print &out) { setOutput(out); }
  /// Support for resampling via write. The audio information is copied from the
  /// io
  ResampleStream(AudioOutput &out) {
    setAudioInfo(out.audioInfo());
    setOutput(out);
  }

  /// Support for resampling via write and read.
  ResampleStream(Stream &io) { setStream(io); }

  /// Support for resampling via write and read. The audio information is copied
  /// from the io
  ResampleStream(AudioStream &io) {
    setAudioInfo(io.audioInfo());
    setStream(io);
  }

  /// Provides the default configuraiton
  ResampleConfig defaultConfig() {
    ResampleConfig cfg;
    cfg.copyFrom(audioInfo());
    return cfg;
  }

  bool begin(ResampleConfig cfg) {
    LOGI("begin step_size: %f", cfg.step_size);
    //is_output_notify = false;
    to_sample_rate = cfg.to_sample_rate;
    out_buffer.resize(cfg.buffer_size);

    setupLastSamples(cfg);
    setStepSize(cfg.step_size);
    is_first = true;
    idx = 0;
    idx_q16 = 0;
    // step_dirty = true;
    bytes_per_frame = info.bits_per_sample / 8 * info.channels;

    setupReader();

    setAudioInfo(cfg);

    return true;
  }

  bool begin(AudioInfo from, AudioInfo to) {
    if (from.bits_per_sample != to.bits_per_sample){
      LOGE("invalid bits_per_sample: %d", (int) to.bits_per_sample);
      return false;
    }
    if (from.channels != to.channels){
      LOGE("invalid channels: %d", (int) to.channels);
      return false;
    }
    return begin(from, (sample_rate_t)to.sample_rate);
  }

  bool begin(AudioInfo from, int toRate) {
    return begin(from, (sample_rate_t)toRate);
  }

  bool begin(AudioInfo from, sample_rate_t toRate) {
    ResampleConfig rcfg;
    rcfg.copyFrom(from);
    rcfg.to_sample_rate = toRate;
    rcfg.step_size = getStepSize(from.sample_rate, toRate);
    return begin(rcfg);
  }

  virtual bool begin(AudioInfo info) {
    if (to_sample_rate != 0) return begin(info, to_sample_rate);
    return begin(info, step_size);
  }

  bool begin() override {
    return begin(audioInfo());
  }

  bool begin(AudioInfo info, float step) {
    ResampleConfig rcfg;
    rcfg.copyFrom(info);
    step_size = step;
    return begin(rcfg);
  }

  void setAudioInfo(AudioInfo newInfo) override {
    // update the step size if a fixed to_sample_rate has been defined
    if (to_sample_rate != 0) {
      setStepSize(getStepSize(newInfo.sample_rate, to_sample_rate));
    }
    // notify about changes
    LOGI("-> ResampleStream:")
    AudioStream::setAudioInfo(newInfo);
  }

  AudioInfo audioInfoOut() override {
    AudioInfo out = audioInfo();
    if (to_sample_rate != 0) {
      out.sample_rate = to_sample_rate;
    } else {
      out.sample_rate = out.sample_rate * step_size;
    }
    return out;
  }

  /// influence the sample rate
  void setStepSize(float step) {
    LOGI("setStepSize: %f", step);
    if (step <= 0.0f) {
      step = 1.0f;
    }
    step_size = step;
    idx_q16 = static_cast<int32_t>(idx * 65536.0f);
    step_q16 = static_cast<int32_t>(step_size * 65536.0f + 0.5f);
    if (step_q16 <= 0) {
      step_q16 = 1;
    }
  }

  void setTargetSampleRate(int rate) { to_sample_rate = rate; }

  /// calculate the step size the sample rate: e.g. from 44200 to 22100 gives a
  /// step size of 2 in order to provide fewer samples
  float getStepSize(float sampleRateFrom, float sampleRateTo) {
    return sampleRateFrom / sampleRateTo;
  }

  /// Returns the actual step size
  float getStepSize() { return step_size; }

  // int availableForWrite() override { return p_print->availableForWrite(); }

  size_t write(const uint8_t *data, size_t len) override {
    LOGD("ResampleStream::write: %d", (int)len);
    //addNotifyOnFirstWrite();
    size_t written = 0;
    switch (info.bits_per_sample) {
      case 16:
        return write<int16_t>(p_print, data, len, written);
      case 24:
        return write<int24_t>(p_print, data, len, written);
      case 32:
        return write<int32_t>(p_print, data, len, written);
      default:
        TRACEE();
    }
    return 0;
  }

  /// Activates buffering to avoid small incremental writes
  void setBuffered(bool active) { is_buffer_active = active; }

  /// When buffering is active, writes the buffered audio to the output
  void flush() PRINT_FLUSH_OVERRIDE {
    if (p_out == nullptr || out_buffer.isEmpty()) return;

    TRACED();
#if USE_PRINT_FLUSH
      p_out->flush();
#endif

    while (!out_buffer.isEmpty()) {
      int to_write = out_buffer.available();
      int rc = p_out->write(out_buffer.data(), to_write);

      if (rc <= 0) {
        LOGE("write error %d vs %d", rc, to_write);
        break;
      }

      if (rc < to_write) {
        LOGW("partial write %d vs %d", rc, to_write);
      }

      out_buffer.clearArray(rc);
    }
  }

  float getByteFactor() override { return 1.0f / step_size; }

 protected:
  Vector<uint8_t> last_samples{0};
  float idx = 0;
  int32_t idx_q16 = 0;
  int32_t step_q16 = 65536;
  bool is_first = true;
  float step_size = 1.0;
  int to_sample_rate = 0;
  int bytes_per_frame = 0;
  // optional buffering
  bool is_buffer_active = USE_RESAMPLE_BUFFER;
  SingleBuffer<uint8_t> out_buffer{0};
  Print *p_out = nullptr;

  /// Sets up the buffer for the rollover samples
  void setupLastSamples(AudioInfo cfg) {
    int bytes_per_sample = cfg.bits_per_sample / 8;
    int last_samples_size = cfg.channels * bytes_per_sample;
    last_samples.resize(last_samples_size);
    memset(last_samples.data(), 0, last_samples_size);
  }

  /// Writes the buffer to defined output after resampling
  template <typename T>
  size_t write(Print *p_out, const uint8_t *buffer, size_t bytes,
               size_t &written) {
    this->p_out = p_out;
    if (step_size == 1.0f) {
      written = p_out->write(buffer, bytes);
      return written;
    }
    // prevent npe
    if (info.channels == 0) {
      LOGE("channels is 0");
      return 0;
    }

    T *data = (T *)buffer;
    int samples = bytes / sizeof(T);
    int channels = info.channels;
    size_t frames = samples / channels;
    written = 0;

    if (sizeof(T) == sizeof(int16_t) && channels == 2) {
      return writeFastInt16Stereo(p_out, reinterpret_cast<int16_t *>(data),
                                  frames, written);
    }

    // avoid noise if audio does not start with 0
    if (is_first) {
      is_first = false;
      setupLastSamples<T>(data, 0);
    }

    T frame[channels];
    size_t frame_size = sizeof(T) * channels;
    T *pt_last_samples = (T *)last_samples.data();

    // process all samples
    while (idx < frames - 1) {
      int frame_idx0 = (int)idx;
      float frac = idx - frame_idx0;
      int frame_idx1 = frame_idx0 + 1;

      T *frame0 = frame_idx0 >= 0 ? &data[frame_idx0 * channels]
                                   : (T *)pt_last_samples;
      T *frame1 = &data[frame_idx1 * channels];

      // fast integer path when we exactly hit a source frame
      if (frac <= 0.000001f) {
        memcpy(frame, frame0, frame_size);
      }
      // optimized fixed-point interpolation for 16-bit samples
      else if (sizeof(T) == sizeof(int16_t)) {
        int32_t frac_q15 = (int32_t)(frac * 32768.0f + 0.5f);
        int32_t one_minus_q15 = 32768 - frac_q15;
        for (int ch = 0; ch < channels; ch++) {
          int32_t v0 = (int32_t)frame0[ch];
          int32_t v1 = (int32_t)frame1[ch];
          int32_t y = (v0 * one_minus_q15 + v1 * frac_q15) >> 15;
          frame[ch] = NumberConverter::clipT<T>((float)y);
        }
      } else {
        // generic interpolation path
        for (int ch = 0; ch < channels; ch++) {
          float v0 = (float)((int32_t)frame0[ch]);
          float v1 = (float)((int32_t)frame1[ch]);
          frame[ch] = NumberConverter::clipT<T>(v0 + (v1 - v0) * frac);
        }
      }

      if (is_buffer_active) {
        // if buffer is full we send it to output
        if (out_buffer.availableForWrite() <= frame_size) {
          flush();
        }

        // we use a buffer to minimize the number of output calls
        int tmp_written =
            out_buffer.writeArray((const uint8_t *)&frame, frame_size);
        written += tmp_written;
        if (frame_size != tmp_written) {
          TRACEE();
        }
      } else {
        int tmp = p_out->write((const uint8_t *)&frame, frame_size);
        written += tmp;
        if (tmp != frame_size) {
          LOGE("Failed to write %d bytes: %d", (int)frame_size, tmp);
        }
      }

      idx += step_size;
    }

    flush();

    // save last samples to be made available at index position -1;
    setupLastSamples<T>(data, frames - 1);
    idx -= frames;
    idx_q16 = static_cast<int32_t>(idx * 65536.0f);

    if (bytes != (written * step_size)) {
      LOGD("write: %d vs %d", (int)bytes, (int)written);
    }

    // returns requested bytes to avoid rewriting of processed bytes
    return frames * channels * sizeof(T);
  }

  size_t writeFastInt16Stereo(Print *p_out, const int16_t *data, size_t frames,
                              size_t &written) {
    if (frames == 0) {
      return 0;
    }

    const size_t frame_size = sizeof(int16_t) * 2;
    int16_t frame[2];
    int16_t *pt_last_samples = reinterpret_cast<int16_t *>(last_samples.data());

    // avoid noise if audio does not start with 0
    if (is_first) {
      is_first = false;
      setupLastSamples<int16_t>(const_cast<int16_t *>(data), 0);
    }

    const int32_t max_idx_q16 = static_cast<int32_t>((frames - 1) << 16);
    while (idx_q16 < max_idx_q16) {
      int32_t frame_idx0 = idx_q16 >> 16;
      int32_t frac_q16 = idx_q16 - (frame_idx0 << 16);
      int32_t frame_idx1 = frame_idx0 + 1;

      const int16_t *frame0 =
          frame_idx0 >= 0 ? &data[frame_idx0 * 2] : pt_last_samples;
      const int16_t *frame1 = &data[frame_idx1 * 2];

      if (frac_q16 == 0) {
        frame[0] = frame0[0];
        frame[1] = frame0[1];
      } else {
        int32_t one_minus_q16 = 65536 - frac_q16;
        frame[0] = static_cast<int16_t>(
            ((static_cast<int32_t>(frame0[0]) * one_minus_q16) +
             (static_cast<int32_t>(frame1[0]) * frac_q16)) >>
            16);
        frame[1] = static_cast<int16_t>(
            ((static_cast<int32_t>(frame0[1]) * one_minus_q16) +
             (static_cast<int32_t>(frame1[1]) * frac_q16)) >>
            16);
      }

      if (is_buffer_active) {
        if (out_buffer.availableForWrite() <= frame_size) {
          flush();
        }
        int tmp_written =
            out_buffer.writeArray(reinterpret_cast<const uint8_t *>(&frame),
                                  frame_size);
        written += tmp_written;
        if (frame_size != static_cast<size_t>(tmp_written)) {
          TRACEE();
        }
      } else {
        int tmp = p_out->write(reinterpret_cast<const uint8_t *>(&frame),
                               frame_size);
        written += tmp;
        if (tmp != static_cast<int>(frame_size)) {
          LOGE("Failed to write %d bytes: %d", (int)frame_size, tmp);
        }
      }

      idx_q16 += step_q16;
    }

    flush();

    setupLastSamples<int16_t>(const_cast<int16_t *>(data), frames - 1);
    idx_q16 -= static_cast<int32_t>(frames << 16);
    idx = static_cast<float>(idx_q16) / 65536.0f;

    return frames * frame_size;
  }

  /// get the interpolated value for indicated (float) index value
  template <typename T>
  T getValue(T *data, float frame_idx, int channel) {
    // interpolate value
    int frame_idx0 = frame_idx;
    // e.g. index -0.5 should be determined from -1 to 0 range
    if (frame_idx0 == 0 && frame_idx < 0) frame_idx0 = -1;
    int frame_idx1 = frame_idx0 + 1;
    T val0 = lookup<T>(data, frame_idx0, channel);
    T val1 = lookup<T>(data, frame_idx1, channel);

    float result = mapT<float>(frame_idx, frame_idx0, frame_idx1, val0, val1);
    LOGD("getValue idx: %d:%d / val: %d:%d / %f -> %f", frame_idx0, frame_idx1,
         (int)val0, (int)val1, frame_idx, result)
    return (float)round(result);
  }

  /// lookup value for indicated frame & channel: index starts with -1;
  template <typename T>
  T lookup(T *data, int frame, int channel) {
    if (frame >= 0) {
      return data[frame * info.channels + channel];
    } else {
      // index -1 (get last sample from previos run)
      T *pt_last_samples = (T *)last_samples.data();
      return pt_last_samples[channel];
    }
  }
  /// store last samples to provide values for index -1
  template <typename T>
  void setupLastSamples(T *data, int frame) {
    for (int ch = 0; ch < info.channels; ch++) {
      T *pt_last_samples = (T *)last_samples.data();
      pt_last_samples[ch] = data[(frame * info.channels) + ch];
      LOGD("setupLastSamples ch:%d - %d", ch, (int)pt_last_samples[ch])
    }
  }
};

}  // namespace audio_tools
