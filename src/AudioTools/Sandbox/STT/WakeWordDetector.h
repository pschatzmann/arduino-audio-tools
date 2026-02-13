#pragma once

#include <algorithm>
#include <cmath>

#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "AudioTools/AudioLibs/AudioFFT.h"

namespace audio_tools {

/*
 * @brief Frame holding the indices of the top 3 frequencies in an FFT window.
 *
 * Used as a compact representation of the dominant frequency content in a frame
 * of audio.
 */
template <size_t N>
struct FrequencyFrame {
  uint16_t top_freqs[N];  ///< Indices of top 3 frequencies in FFT
};

/**
 * @class WakeWordDetector
 * @brief Template-based wake word detector for microcontrollers using dominant
 * frequency patterns.
 *
 * This class detects wake words by comparing the sequence of the top N dominant
 * frequencies in each audio frame to stored templates for each wake word. When
 * the percentage of matching frames exceeds a configurable threshold, the
 * corresponding wake word is considered detected.
 *
 * @tparam N Number of dominant frequencies to track per frame (default: 3)
 *
 * Usage:
 * - Record each wake word and extract the top N frequencies for each frame to
 * build templates.
 * - Instantiate WakeWordDetector<N> and add templates for each wake word.
 * - Register a callback to handle detection events using setWakeWordCallback().
 *
 * Example:
 * @code
 * audio_tools::WakeWordDetector<3> detector(fft);
 * detector.addTemplate(my_template_frames, 80.0f, "hello");
 * detector.setWakeWordCallback([](const char* name) { Serial.println(name); });
 * // ...
 * @endcode
 */
template <typename T = int16_t, size_t N = 3>
class WakeWordDetector : public AudioOutput {
 public:
  struct Template {
    Vector<FrequencyFrame<N>>
        frames;  ///< Sequence of frequency frames for the wake word
    float threshold_percent;  ///< Minimum percent of matching frames required
                              ///< for detection (0-100)
    const char* name;         ///< Name/label of the wake word
    float last_match_percent =
        0.0f;  ///< Last computed match percent for this template
  };

  using WakeWordCallback = void (*)(const char* name);

  WakeWordDetector(AudioFFTBase& fft)
      : p_fft(&fft) {
    _frame_pos = 0;
    auto& fft_cfg = fft.config();
    fft_cfg.ref = this;
    fft_cfg.callback = fftResult;
  }

  void startRecording() {
    _recent_frames.clear();
    _is_recording = true;
  }

  Vector<FrequencyFrame<N>> stopRecording() {
    _is_recording = false;
    return _recent_frames;
  }

  bool isRecording() const { return _is_recording; }

  void addTemplate(const Vector<FrequencyFrame<N>>& frames,
                   float threshold_percent, const char* name) {
    Template t;
    t.frames = frames;
    t.threshold_percent = threshold_percent;
    t.name = name;
    t.last_match_percent = 0.0f;
    _templates.push_back(t);
    if (frames.size() > _max_template_len) _max_template_len = frames.size();
  }

  void setWakeWordCallback(WakeWordCallback cb) { _callback = cb; }

  size_t write(const uint8_t* buf, size_t size) override {
    return p_fft->write(buf, size);
  }

  static void fftResult(AudioFFTBase& fft) {
    // This static method must access instance data via fft.config().ref
    auto* self = static_cast<WakeWordDetector<T,N>*>(fft.config().ref);
    if (!self) return;
    FrequencyFrame<N> frame;
    AudioFFTResult result[N];
      fft.resultArray(result);
    for (size_t j = 0; j < N; j++) {
      frame.top_freqs[j] = result[j].frequency;
    }
    self->_recent_frames.push_back(frame);

    if (self->_is_recording) {
      return;
    }

    if (self->_recent_frames.size() > self->_max_template_len)
      self->_recent_frames.erase(self->_recent_frames.begin());
    for (size_t i = 0; i < self->_templates.size(); ++i) {
      Template& tmpl = self->_templates[i];
      if (self->_recent_frames.size() >= tmpl.frames.size()) {
        float percent = self->matchTemplate(tmpl);
        if (percent >= tmpl.threshold_percent) {
          if (self->_callback) self->_callback(tmpl.name);
        }
      }
    }
  }

 protected:
  Vector<Template> _templates;               ///< List of wake word templates
  Vector<FrequencyFrame<N>> _recent_frames;  ///< Recent frames for comparison
  Vector<T> _buffer;  ///< Buffer for incoming PCM samples
  AudioFFTBase* p_fft = nullptr;
  bool _is_recording = false;    ///< True if currently recording a template
  size_t _frame_pos;             ///< Current position in frame buffer
  size_t _max_template_len = 0;  ///< Length of the longest template
  WakeWordCallback _callback = nullptr;

  float matchTemplate(Template& tmpl) {
    size_t matches = 0;
    size_t offset = _recent_frames.size() - tmpl.frames.size();
    for (size_t i = 0; i < tmpl.frames.size(); ++i) {
      size_t frame_matches = 0;
      for (size_t j = 0; j < N; ++j) {
        if (tmpl.frames[i].top_freqs[j] ==
            _recent_frames[offset + i].top_freqs[j])
          frame_matches++;
      }
      if (frame_matches >= (N >= 2 ? N - 1 : 1))  // at least N-1 out of N match
        matches++;
    }
    float percent = (tmpl.frames.size() > 0)
                        ? (100.0f * matches / tmpl.frames.size())
                        : 0.0f;
    tmpl.last_match_percent = percent;
    return percent;
  }
};

}  // namespace audio_tools