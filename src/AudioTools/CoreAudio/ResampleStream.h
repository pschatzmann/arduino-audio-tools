#pragma once

#include "AudioTools/CoreAudio/AudioIO.h"

#if USE_PRINT_FLUSH
#  define PRINT_FLUSH_OVERRIDE override
#else
#  define PRINT_FLUSH_OVERRIDE
#endif

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
#if PREFER_FIXEDPOINT
    idx_fixed = 0;
    // Q16.16 step_size_fixed is an int32_t: a ratio at or beyond +/-32768
    // overflows it. Real resampling ratios are nowhere near this (e.g.
    // 192000/8000 = 24), so this only catches misconfiguration.
    if (cfg.step_size >= 32768.0f || cfg.step_size <= -32768.0f) {
      LOGE("step_size %f is out of range for fixed point (+/-32768) - falling back is not implemented, output will wrap", cfg.step_size);
    }
#endif
    // step_dirty = true;
    bytes_per_frame = info.bits_per_sample / 8 * info.channels;
    carry_len = 0;

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
    step_size = step;
#if PREFER_FIXEDPOINT
    step_size_fixed = (int32_t) lround(step * 65536.0f);
#endif
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
    int frame_bytes = info.bits_per_sample / 8 * info.channels;
    if (frame_bytes <= 0) {
      TRACEE();
      return 0;
    }

    const uint8_t *proc_data = data;
    size_t proc_len = len;
    // Combine any partial-frame remainder carried over from a previous
    // call (e.g. the source delivered a byte count that wasn't a whole
    // number of frames - a short/malformed USB packet, for instance) with
    // the new data, instead of processing len on its own. write<T>()/
    // writeFixed() below only ever consume whole frames; without this,
    // any remainder they leave behind would be silently dropped by the
    // caller (see TransformationReader::fillResultQueue(), which advances
    // past exactly `len` bytes on the source regardless of how much we
    // report back) - permanently shifting every following sample by the
    // gap and turning a single hiccup into session-long scrambled audio.
    if (carry_len > 0) {
      combine_buffer.resize(carry_len + len);
      memcpy(combine_buffer.data(), carry.data(), carry_len);
      memcpy(combine_buffer.data() + carry_len, data, len);
      proc_data = combine_buffer.data();
      proc_len = combine_buffer.size();
    }

    size_t whole = (proc_len / (size_t)frame_bytes) * (size_t)frame_bytes;
    size_t leftover = proc_len - whole;

    if (whole > 0) {
      size_t written = 0;
      switch (info.bits_per_sample) {
        case 16:
#if PREFER_FIXEDPOINT
          writeFixed(p_print, proc_data, whole, written);
#else
          write<int16_t>(p_print, proc_data, whole, written);
#endif
          break;
        case 24:
          write<int24_t>(p_print, proc_data, whole, written);
          break;
        case 32:
          write<int32_t>(p_print, proc_data, whole, written);
          break;
        default:
          TRACEE();
          return 0;
      }
    }

    carry_len = leftover;
    if (leftover > 0) {
      carry.resize(leftover);
      memcpy(carry.data(), proc_data + whole, leftover);
    }

    // The remainder is safely carried forward (to be completed by the
    // next call) rather than dropped, so all of the caller's new bytes
    // count as accounted for.
    return len;
  }

  /// Activates buffering to avoid small incremental writes
  void setBuffered(bool active) { is_buffer_active = active; }

  /// When buffering is active, writes the buffered audio to the output
  void flush() PRINT_FLUSH_OVERRIDE {
    if (p_out != nullptr && !out_buffer.isEmpty()) {
      TRACED();
#if USE_PRINT_FLUSH
      p_out->flush();
#endif
      int rc = p_out->write(out_buffer.data(), out_buffer.available());
      if (rc != out_buffer.available()) {
        LOGE("write error %d vs %d", rc, out_buffer.available());
      }
      out_buffer.reset();
    }
  }

  float getByteFactor() override { return 1.0f / step_size; }

 protected:
  Vector<uint8_t> last_samples{0};
  float idx = 0;
  bool is_first = true;
  float step_size = 1.0;
  int to_sample_rate = 0;
  int bytes_per_frame = 0;
  // Partial-frame remainder left over from a write() whose byte count
  // wasn't a whole number of frames - see write() for why this must be
  // carried forward rather than dropped.
  Vector<uint8_t> carry{0};
  size_t carry_len = 0;
  Vector<uint8_t> combine_buffer{0};  // scratch space for carry+new data
#if PREFER_FIXEDPOINT
  // Q16.16 fixed-point read position and step size, used for the 16-bit fast
  // path: avoids soft-float add/div/round() on FPU-less MCUs (e.g. RP2040).
  int32_t idx_fixed = 0;
  int32_t step_size_fixed = 1 << 16;
#endif
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
    size_t frames = samples / info.channels;
    written = 0;

    // avoid noise if audio does not start with 0
    if (is_first) {
      is_first = false;
      setupLastSamples<T>(data, 0);
    }

    T frame[info.channels];
    size_t frame_size = sizeof(frame);

    // process all samples
    while (idx < frames - 1) {
      for (int ch = 0; ch < info.channels; ch++) {
        T result = getValue<T>(data, idx, ch);
        frame[ch] = result;
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

    if (bytes != (written * step_size)) {
      LOGD("write: %d vs %d", (int)bytes, (int)written);
    }

    // returns requested bytes to avoid rewriting of processed bytes
    return frames * info.channels * sizeof(T);
  }

#if PREFER_FIXEDPOINT
  /// Fixed-point (Q16.16) equivalent of write<int16_t>(): avoids float
  /// add/div/round() in the per-sample loop on FPU-less MCUs (e.g. RP2040).
  size_t writeFixed(Print *p_out, const uint8_t *buffer, size_t bytes,
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
    int16_t *data = (int16_t *)buffer;
    int samples = bytes / sizeof(int16_t);
    size_t frames = samples / info.channels;
    written = 0;

    // avoid noise if audio does not start with 0
    if (is_first) {
      is_first = false;
      setupLastSamples<int16_t>(data, 0);
    }

    int16_t frame[info.channels];
    size_t frame_size = sizeof(frame);

    int32_t frames_fixed = ((int32_t)frames - 1) << 16;
    // process all samples
    while (idx_fixed < frames_fixed) {
      for (int ch = 0; ch < info.channels; ch++) {
        frame[ch] = getValueFixed(data, idx_fixed, ch);
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

      idx_fixed += step_size_fixed;
    }

    flush();

    // save last samples to be made available at index position -1;
    setupLastSamples<int16_t>(data, frames - 1);
    idx_fixed -= (int32_t)frames << 16;

    // returns requested bytes to avoid rewriting of processed bytes
    return frames * info.channels * sizeof(int16_t);
  }

  /// Fixed-point (Q16.16 position / Q0.8 interpolation weight) equivalent of
  /// getValue<int16_t>().
  int16_t getValueFixed(int16_t *data, int32_t frame_idx_fixed, int channel) {
    // arithmetic shift floors toward -infinity, which correctly handles the
    // [-1, 0) range without the truncate-then-patch needed for float
    int32_t frame_idx0 = frame_idx_fixed >> 16;
    int32_t frame_idx1 = frame_idx0 + 1;
    int16_t val0 = lookup<int16_t>(data, frame_idx0, channel);
    int16_t val1 = lookup<int16_t>(data, frame_idx1, channel);

    // top 8 bits of the 16-bit fraction: keeps diff * weight within 32 bits
    // (a single MULS on Cortex-M0+) instead of needing a 64-bit intermediate
    int32_t weight = (frame_idx_fixed >> 8) & 0xFF;
    int32_t diff = (int32_t)val1 - (int32_t)val0;
    // round-half-up instead of floor, to avoid a systematic bias
    int32_t result = (int32_t)val0 + (((diff * weight) + (1 << 7)) >> 8);
    return (int16_t)result;
  }
#endif

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

    // direct 2-point lerp: frame_idx1 - frame_idx0 is always 1, so the
    // generic mapT() divide by (in_max - in_min) is a wasted division here;
    // and a manual +/-0.5 round avoids a libm round() call.
    float frac = frame_idx - frame_idx0;
    float result = val0 + frac * (val1 - val0);
    LOGD("getValue idx: %d:%d / val: %d:%d / %f -> %f", frame_idx0, frame_idx1,
         (int)val0, (int)val1, frame_idx, result)
    return (T)(result + (result >= 0 ? 0.5f : -0.5f));
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
