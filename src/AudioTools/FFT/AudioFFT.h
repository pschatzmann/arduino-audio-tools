#pragma once

#include "AudioTools/FFT/FFTWindows.h"
#include "AudioTools/CoreAudio/AudioStreams.h"
#include "AudioTools/CoreAudio/MusicalNotes.h"

/**
 * @defgroup fft FFT
 * @ingroup dsp
 * @brief Fast Fourier Transform
 **/

namespace audio_tools {

// forward declaration
class AudioFFTBase;
static MusicalNotes AudioFFTNotes;

/**
 * @brief Result of the FFT
 * @ingroup fft
 */
struct AudioFFTResult {
  int bin = 0;
  float magnitude = 0.0f;
  float frequency = 0.0f;

  int frequencyAsInt() { return round(frequency); }
  const char *frequencyAsNote() { return AudioFFTNotes.note(frequency); }
  const char *frequencyAsNote(float &diff) {
    return AudioFFTNotes.note(frequency, diff);
  }
};

/**
 * @brief Configuration for AudioFFT. If there are more then 1 channel the
 * channel_used is defining which channel is used to perform the fft on.
 * @note Only channel_used is analyzed by the forward FFT - the other
 * channels' input samples are discarded. On the reverse FFT the
 * reconstructed signal is written identically to all `channels`, so
 * multi-channel input/output is effectively collapsed to dual-mono rather
 * than processed independently per channel.
 * @ingroup fft
 */
struct AudioFFTConfig : public AudioInfo {
  AudioFFTConfig() {
    channels = 2;
    bits_per_sample = 16;
    sample_rate = 44100;
  }
  /// Callback method which is called after we got a new result
  void (*callback)(AudioFFTBase &fft) = nullptr;
  /// Channel which is used as input
  uint8_t channel_used = 0;
  int length = 8192;
  int stride = 0;
  /// Optional window function for both fft and ifft
  WindowFunction *window_function = nullptr;
  /// Optional window function for fft only
  WindowFunction *window_function_fft = nullptr;
  /// Optional window function for ifft only
  WindowFunction *window_function_ifft = nullptr;
  /// TX_MODE = FFT, RX_MODE = IFFT
  RxTxMode rxtx_mode = TX_MODE;
  /// caller
  void *ref = nullptr;
};

/// And individual FFT Bin
struct FFTBin {
  float real;
  float img;

  FFTBin() = default;

  FFTBin(float r, float i) {
    real = r;
    img = i;
  }

  void multiply(float f) {
    real *= f;
    img *= f;
  }

  void conjugate() { img = -img; }

  void clear() { real = img = 0.0f; }
};

/// Inverse FFT Overlapp Add
class FFTInverseOverlapAdder {
 public:
  FFTInverseOverlapAdder(int size = 0) {
    if (size > 0) resize(size);
  }

  /// Initilze data by defining new size
  bool resize(size_t size) {
    // reset max for new scaling
    rfft_max = 0.0;
    // define new size
    len = size;
    // Vector::resize() also returns false if the size did not change (e.g.
    // resize(0) from end() when the inverse FFT was never used)
    data.resize(size);
    if (size > 0 && data.data() == nullptr) {
      LOGE("Could not resize data");
      return false;
    }
    for (int j = 0; j < data.size(); j++) {
      data[j] = 0.0;
    }
    return true;
  }

  // adds the values to the array (by applying the window function)
  void add(float value, int pos, WindowFunction *window_function) {
    float add_value = value;
    if (window_function != nullptr) {
      add_value = value * window_function->factor(pos);
    }
    assert(pos < len);
    data[pos] += add_value;
  }

  // gets the scaled audio data as result
  void getStepData(float *result, int stride, float maxResult) {
    for (int j = 0; j < stride; j++) {
      // determine max value to scale
      if (data[j] > rfft_max) rfft_max = data[j];
    }
    for (int j = 0; j < stride; j++) {
      result[j] = data[j] / rfft_max * maxResult;
      // clip
      if (result[j] > maxResult) {
        result[j] = maxResult;
      }
      if (result[j] < -maxResult) {
        result[j] = -maxResult;
      }
    }
    // copy data to head
    for (int j = 0; j < len - stride; j++) {
      data[j] = data[j + stride];
    }
    // clear tail
    for (int j = len - stride; j < len; j++) {
      data[j] = 0.0;
    }
  }

  /// provides the actual size
  int size() { return data.size(); }

 protected:
  Vector<float> data{0};
  int len = 0;
  float rfft_max = 0;
};

/**
 * @brief Abstract Class which defines the basic FFT functionality
 * @ingroup fft
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriver {
 public:
  virtual ~FFTDriver() = default;
  virtual bool begin(int len) = 0;
  virtual void end() = 0;
  /// Sets the real value
  virtual void setValue(int pos, float value) = 0;
  /// Perform FFT
  virtual void fft() = 0;
  /// Calculate the magnitude (fft result) at index (sqr(i² + r²))
  virtual float magnitude(int idx) = 0;
  /// Calculate the magnitude w/o sqare root
  virtual float magnitudeFast(int idx) = 0;
  virtual bool isValid() = 0;
  /// Returns true if reverse FFT is supported
  virtual bool isReverseFFT() { return false; }
  /// Calculate reverse FFT
  virtual void rfft() { LOGE("Not implemented"); }
  /// Get result value from Reverse FFT
  virtual float getValue(int pos) = 0;
  /// sets the value of a bin
  virtual bool setBin(int idx, float real, float img) { return false; }
  /// sets the value of a bin
  bool setBin(int pos, FFTBin &bin) { return setBin(pos, bin.real, bin.img); }
  /// gets the value of a bin
  virtual bool getBin(int pos, FFTBin &bin) { return false; }
};

/**
 * @brief Executes FFT using audio data privded by write() and/or an inverse FFT
 * where the samples are made available via readBytes(). The Driver which is
 * passed in the constructor selects a specifc FFT implementation.
 * @ingroup fft
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFTBase : public AudioStream {
 public:
  /// Default Constructor. The len needs to be of the power of 2 (e.g. 512,
  /// 1024, 2048, 4096, 8192)
  /// Takes ownership of the driver (allocated with new), which is deleted
  /// together with this object
  AudioFFTBase(FFTDriver *driver) { p_driver = driver; }
  AudioFFTBase(const AudioFFTBase &) = delete;
  AudioFFTBase &operator=(const AudioFFTBase &) = delete;

  ~AudioFFTBase() {
    end();
    delete p_driver;
  }

  /// Provides the default configuration
  AudioFFTConfig defaultConfig(RxTxMode mode = TX_MODE) {
    AudioFFTConfig info;
    info.rxtx_mode = mode;
    return info;
  }

  /// starts the processing
  bool begin(AudioFFTConfig info) {
    cfg = info;
    return begin();
  }

  /// starts the processing
  bool begin() override {
    bins = cfg.length / 2;
    // define window functions
    if (cfg.window_function_fft == nullptr)
      cfg.window_function_fft = cfg.window_function;
    if (cfg.window_function_ifft == nullptr)
      cfg.window_function_ifft = cfg.window_function;
    // define default stride value if not defined
    if (cfg.stride == 0) cfg.stride = cfg.length;

    if (!isPowerOfTwo(cfg.length)) {
      LOGE("Len must be of the power of 2: %d", cfg.length);
      return false;
    }
    if (!p_driver->begin(cfg.length)) {
      LOGE("Not enough memory");
    }

    if (cfg.window_function_fft != nullptr) {
      cfg.window_function_fft->begin(cfg.length);
    }
    if (cfg.window_function_ifft != nullptr &&
        cfg.window_function_ifft != cfg.window_function_fft) {
      cfg.window_function_ifft->begin(cfg.length);
    }

    bool is_valid_rxtx = false;
    if (cfg.rxtx_mode == TX_MODE || cfg.rxtx_mode == RXTX_MODE) {
      // holds last N bytes that need to be reprocessed
      stride_buffer.resize((cfg.length) * bytesPerSample());
      is_valid_rxtx = true;
    }
    if (cfg.rxtx_mode == RX_MODE || cfg.rxtx_mode == RXTX_MODE) {
      rfft_data.resize(cfg.channels * bytesPerSample() * cfg.stride);
      rfft_add.resize(cfg.length);
      step_data.resize(cfg.stride);
      is_valid_rxtx = true;
    }

    if (!is_valid_rxtx) {
      LOGE("Invalid rxtx_mode");
      return false;
    }

    current_pos = 0;
    return p_driver->isValid();
  }

  /// Just resets the current_pos e.g. to start a new cycle
  void reset() {
    current_pos = 0;
    // drop partially collected input so the next frame starts from scratch
    stride_buffer.reset();
    if (cfg.window_function_fft != nullptr) {
      cfg.window_function_fft->begin(cfg.length);
    }
    if (cfg.window_function_ifft != nullptr) {
      cfg.window_function_ifft->begin(cfg.length);
    }
  }

  operator bool() override {
    return p_driver != nullptr && p_driver->isValid();
  }

  /// Notify change of audio information
  void setAudioInfo(AudioInfo info) override {
    cfg.bits_per_sample = info.bits_per_sample;
    cfg.sample_rate = info.sample_rate;
    cfg.channels = info.channels;
    begin(cfg);
  }

  /// Release the allocated memory
  void end() override {
    p_driver->end();
    l_magnitudes.resize(0);
    rfft_data.resize(0);
    rfft_add.resize(0);
    step_data.resize(0);
    mel_filters.resize(0);
    mel_weights.resize(0);
    mel_n_bins = 0;
  }

  /// Provide the audio data as FFT input
  size_t write(const uint8_t *data, size_t len) override {
    size_t result = 0;
    if (p_driver->isValid()) {
      result = len;
      switch (cfg.bits_per_sample) {
        case 8:
          processSamples<int8_t>(data, len);
          break;
        case 16:
          processSamples<int16_t>(data, len / 2);
          break;
        case 24:
          processSamples<int24_t>(data, len / 3);
          break;
        case 32:
          processSamples<int32_t>(data, len / 4);
          break;
        default:
          LOGE("Unsupported bits_per_sample: %d", cfg.bits_per_sample);
          break;
      }
    }
    return result;
  }

  /// Provides the result of a reverse FFT
  size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (rfft_data.size() == 0) return 0;

    // get data via callback if there is no more data
    if (cfg.rxtx_mode == RX_MODE && cfg.callback != nullptr &&
        rfft_data.available() == 0) {
      cfg.callback(*this);
    }

    // execute rfft when we consumed all data
    if (has_rfft_data && rfft_data.available() == 0) {
      rfft();
    }
    return rfft_data.readArray(data, len);
  }

  /// We try to fill the buffer at once
  int availableForWrite() override {
    return cfg.length * cfg.channels * bytesPerSample();
  }

  /// Data available for reverse fft
  int available() override {
    assert(cfg.stride != 0);
    return cfg.stride * cfg.channels * bytesPerSample();
  }

  /// The number of bins used by the FFT which are relevant for the result
  int size() { return bins; }

  /// The number of samples
  int length() { return cfg.length; }

  /// time after the fft: time when the last result was provided - you can poll
  /// this to check if we have a new result
  unsigned long resultTime() { return timestamp; }
  /// time before the fft
  unsigned long resultTimeBegin() { return timestamp_begin; }

  /// Determines the result values in the max magnitude bin
  AudioFFTResult result() {
    AudioFFTResult ret_value;
    ret_value.magnitude = 0.0f;
    ret_value.bin = 0;
    // find max value and index
    for (int j = 0; j < size(); j++) {
      float m = magnitude(j);
      if (m > ret_value.magnitude) {
        ret_value.magnitude = m;
        ret_value.bin = j;
      }
    }
    ret_value.frequency = frequency(ret_value.bin);
    return ret_value;
  }

  /// Determines the N biggest result values
  template <int N>
  void resultArray(AudioFFTResult (&result)[N]) {
    // initialize to negative value
    for (int j = 0; j < N; j++) {
      result[j].magnitude = -1000000;
    }
    // find top n values
    AudioFFTResult act;
    for (int j = 0; j < size(); j++) {
      act.magnitude = magnitude(j);
      act.bin = j;
      act.frequency = frequency(j);
      insertSorted<N>(result, act);
    }
  }

  /// Convert the FFT result to MEL spectrum. The triangular filterbank uses
  /// fractional (HTK style) weights, so no filter collapses to zero width,
  /// and it is computed once and cached until the parameters change.
  /// @param n_bins number of mel bands (<= 0: size())
  /// @param min_freq lower edge in Hz (<= 0: frequency(0))
  /// @param max_freq upper edge in Hz (<= 0: frequency(size() - 1))
  /// @param power use the power spectrum |X|^2 instead of the magnitude |X|
  float *toMEL(int n_bins, float min_freq = 0.0f, float max_freq = 0.0f,
               bool power = false) {
    if (n_bins <= 0) n_bins = size();
    if (!updateMelFilterbank(n_bins, min_freq, max_freq)) return nullptr;
    mel_bins.resize(n_bins);
    // one pass over the bins (each bin belongs to up to two filters)
    const float *mags = power ? magnitudesFast() : magnitudes();
    for (int i = 0; i < n_bins; i++) {
      const MelFilter &filter = mel_filters[i];
      const float *weights = mel_weights.data() + filter.weight_offset;
      const float *m = mags + filter.start_bin;
      float mel_sum = 0.0f;
      for (int j = 0; j < filter.count; j++) mel_sum += m[j] * weights[j];
      mel_bins[i] = mel_sum;
    }
    return mel_bins.data();
  }

  /**
   * @brief Convert MEL spectrum back to linear frequency spectrum, using the
   * same (cached) filterbank as toMEL().
   *
   * @param values Pointer to MEL spectrum values
   * @param n_bins Number of MEL bins
   * @return bool Success status
   */
  bool fromMEL(float *values, int n_bins, float min_freq = 0.0f,
               float max_freq = 0.0f) {
    if (n_bins <= 0 || values == nullptr) return false;
    if (!updateMelFilterbank(n_bins, min_freq, max_freq)) return false;

    // Distribute MEL energy back to linear frequency bins
    Vector<float> linear_magnitudes;
    linear_magnitudes.resize(bins);
    for (int i = 0; i < bins; i++) linear_magnitudes[i] = 0.0f;
    for (int i = 0; i < n_bins; i++) {
      const MelFilter &filter = mel_filters[i];
      const float *weights = mel_weights.data() + filter.weight_offset;
      for (int j = 0; j < filter.count; j++) {
        linear_magnitudes[filter.start_bin + j] += values[i] * weights[j];
      }
    }

    // Set magnitude values and create simple phase (all zeros)
    for (int i = 0; i < bins; i++) {
      FFTBin bin;
      bin.clear();
      bin.real = linear_magnitudes[i];
      setBin(i, bin);
    }
    return true;
  }

  /// provides access to the FFTDriver which implements the basic FFT
  /// functionality
  FFTDriver *driver() { return p_driver; }

  /// Determines the frequency of the indicated bin
  float frequency(int bin) {
    if (bin >= bins) {
      LOGE("Invalid bin %d", bin);
      return 0;
    }
    return static_cast<float>(bin) * cfg.sample_rate / cfg.length;
  }

  /// Determine the bin number from the frequency
  int frequencyToBin(int freq) {
    int max_freq = cfg.sample_rate / 2;
    return map(freq, 0, max_freq, 0, size());
  }

  /// Calculates the magnitude of the fft result to determine the max value (bin
  /// is 0 to size())
  float magnitude(int bin) {
    if (bin >= bins) {
      LOGE("Invalid bin %d", bin);
      return 0;
    }
    return p_driver->magnitude(bin);
  }

  float magnitudeFast(int bin) {
    if (bin >= bins) {
      LOGE("Invalid bin %d", bin);
      return 0;
    }
    return p_driver->magnitudeFast(bin);
  }

  /// calculates the phase
  float phase(int bin) {
    FFTBin fft_bin;
    getBin(bin, fft_bin);
    return atan2(fft_bin.img, fft_bin.real);
  }

  /// Provides the magnitudes as array of size size(). Please note that this
  /// method is allocating additinal memory!
  float *magnitudes() {
    if (l_magnitudes.size() == 0) {
      l_magnitudes.resize(size());
    }
    for (int j = 0; j < size(); j++) {
      l_magnitudes[j] = magnitude(j);
    }
    return l_magnitudes.data();
  }

  /// Provides the magnitudes w/o calling the square root function as array of
  /// size size(). Please note that this method is allocating additinal memory!
  float *magnitudesFast() {
    if (l_magnitudes.size() == 0) {
      l_magnitudes.resize(size());
    }
    for (int j = 0; j < size(); j++) {
      l_magnitudes[j] = magnitudeFast(j);
    }
    return l_magnitudes.data();
  }

  /// sets the value of a bin
  bool setBin(int idx, float real, float img) {
    has_rfft_data = true;
    if (idx < 0 || idx >= size()) return false;
    bool rc_first_half = p_driver->setBin(idx, real, img);
    bool rc_2nd_half = p_driver->setBin(cfg.length - idx, real, img);
    return rc_first_half && rc_2nd_half;
  }
  /// sets the value of a bin
  bool setBin(int pos, FFTBin &bin) { return setBin(pos, bin.real, bin.img); }
  /// gets the value of a bin
  bool getBin(int pos, FFTBin &bin) { return p_driver->getBin(pos, bin); }

  /// clears the fft data
  void clearBins() {
    FFTBin empty{0, 0};
    for (int j = 0; j < size(); j++) {
      setBin(j, empty);
    }
  }

  /// Provides the actual configuration
  AudioFFTConfig &config() { return cfg; }

  /// Provides the reference pointer
  template <typename T>
  T& reference() {
    return *((T*)cfg.ref);
  }

 protected:
  FFTDriver *p_driver = nullptr;
  int current_pos = 0;
  int bins = 0;
  unsigned long timestamp_begin = 0l;
  unsigned long timestamp = 0l;
  AudioFFTConfig cfg;
  FFTInverseOverlapAdder rfft_add{0};
  Vector<float> l_magnitudes{0};
  Vector<float> step_data{0};
  Vector<float> mel_bins{0};
  /// One triangular mel filter: weights for bins [start_bin, start_bin+count)
  struct MelFilter {
    int start_bin = 0;
    int count = 0;
    int weight_offset = 0;
  };
  Vector<MelFilter> mel_filters{0};
  Vector<float> mel_weights{0};
  int mel_n_bins = 0;
  float mel_min_freq = -1.0f;
  float mel_max_freq = -1.0f;
  int mel_length = 0;
  int mel_sample_rate = 0;

  static float hzToMel(float hz) { return 2595.0f * log10f(1.0f + hz / 700.0f); }
  static float melToHz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
  }

  /// (Re)builds the cached sparse filterbank if any parameter changed
  bool updateMelFilterbank(int n_bins, float min_freq, float max_freq) {
    if (bins <= 0 || cfg.sample_rate <= 0) return false;
    if (min_freq <= 0.0f) min_freq = frequency(0);
    if (max_freq <= 0.0f) max_freq = frequency(size() - 1);
    if (n_bins == mel_n_bins && min_freq == mel_min_freq &&
        max_freq == mel_max_freq && cfg.length == mel_length &&
        (int)cfg.sample_rate == mel_sample_rate)
      return true;

    float min_mel = hzToMel(min_freq);
    float mel_step = (hzToMel(max_freq) - min_mel) / (n_bins + 1);
    float bin_hz = (float)cfg.sample_rate / cfg.length;

    // band edges (n_bins + 2 points) and the bin range of every filter
    Vector<float> edges;
    edges.resize(n_bins + 2);
    for (int i = 0; i < n_bins + 2; i++) edges[i] = melToHz(min_mel + i * mel_step);
    mel_filters.resize(n_bins);
    int total = 0;
    for (int i = 0; i < n_bins; i++) {
      MelFilter &filter = mel_filters[i];
      // bins strictly inside (left, right)
      int first = (int)(edges[i] / bin_hz) + 1;
      int last = (int)ceilf(edges[i + 2] / bin_hz) - 1;
      if (last >= bins) last = bins - 1;
      if (first <= last) {
        filter.start_bin = first;
        filter.count = last - first + 1;
      } else {
        // narrower than one FFT bin: use the bin closest to the center so
        // the band still carries energy instead of being constant zero
        int j = (int)(edges[i + 1] / bin_hz + 0.5f);
        filter.start_bin = j >= bins ? bins - 1 : j;
        filter.count = 0;
      }
      filter.weight_offset = total;
      total += filter.count > 0 ? filter.count : 1;
    }
    // one allocation for all weights
    mel_weights.resize(total);
    for (int i = 0; i < n_bins; i++) {
      MelFilter &filter = mel_filters[i];
      float *w = mel_weights.data() + filter.weight_offset;
      if (filter.count == 0) {
        w[0] = 1.0f;
        filter.count = 1;
        continue;
      }
      float left = edges[i], center = edges[i + 1], right = edges[i + 2];
      for (int k = 0; k < filter.count; k++) {
        float f = (filter.start_bin + k) * bin_hz;
        w[k] = f <= center ? (f - left) / (center - left) : (right - f) / (right - center);
      }
    }
    mel_n_bins = n_bins;
    mel_min_freq = min_freq;
    mel_max_freq = max_freq;
    mel_length = cfg.length;
    mel_sample_rate = cfg.sample_rate;
    return true;
  }
  SingleBuffer<uint8_t> stride_buffer{0};
  RingBuffer<uint8_t> rfft_data{0};
  bool has_rfft_data = false;

  // Add samples to input data p_x - and process them if full
  template <typename T>
  void processSamples(const void *data, size_t count) {
    T *dataT = (T *)data;
    if (cfg.channels == 1) {
      // mono: copy contiguous runs instead of one sample per call
      size_t pos = 0;
      while (pos < count) {
        size_t room = stride_buffer.availableForWrite() / sizeof(T);
        size_t n = count - pos < room ? count - pos : room;
        stride_buffer.writeArray((uint8_t *)(dataT + pos), n * sizeof(T));
        pos += n;
        if (stride_buffer.isFull()) processStrideBuffer<T>();
      }
      return;
    }
    T sample;
    for (int j = 0; j < count; j += cfg.channels) {
      sample = dataT[j + cfg.channel_used];
      if (writeStrideBuffer((uint8_t *)&sample, sizeof(T))) processStrideBuffer<T>();
    }
  }

  /// FFT of the full stride buffer, then drop `stride` samples
  template <typename T>
  void processStrideBuffer() {
    T *samples = (T *)stride_buffer.data();
    int sample_count = stride_buffer.size() / sizeof(T);
    assert(sample_count == cfg.length);
    const float scale = 1.0f / NumberConverter::maxValueT<T>();
    for (int j = 0; j < sample_count; j++) {
      T windowed_sample = windowedSample(samples[j], j);
      p_driver->setValue(j, scale * windowed_sample);
    }

    fft<T>();

    // remove stride samples
    stride_buffer.clearArray(cfg.stride * sizeof(T));

    // validate available data in stride buffer
    if (cfg.stride == cfg.length) assert(stride_buffer.available() == 0);
  }


  template <typename T>
  T windowedSample(T sample, int pos) {
    T result = sample;
    if (cfg.window_function_fft != nullptr) {
      result = cfg.window_function_fft->factor(pos) * sample;
    }
    return result;
  }

  template <typename T>
  void fft() {
    timestamp_begin = millis();
    p_driver->fft();
    has_rfft_data = true;
    timestamp = millis();
    if (cfg.callback != nullptr) {
      cfg.callback(*this);
    }
  }

  /// reverse fft
  void rfft() {
    TRACED();
    // execute reverse fft
    p_driver->rfft();
    has_rfft_data = false;
    // add data to sum buffer
    for (int j = 0; j < cfg.length; j++) {
      float value = p_driver->getValue(j);
      rfft_add.add(value, j, cfg.window_function_ifft);
    }
    // get result data from sum buffer
    rfftWriteData(rfft_data);
  }

  /// write reverse fft result to buffer to make it available for readBytes
  void rfftWriteData(BaseBuffer<uint8_t> &data) {
    // get data to result buffer
    // for (int j = 0; j < cfg.stride; j++) {
    //   step_data[j] = 0.0;
    // }
    rfft_add.getStepData(step_data.data(), cfg.stride,
                         NumberConverter::maxValue(cfg.bits_per_sample));

    switch (cfg.bits_per_sample) {
      case 8:
        writeIFFT<int8_t>(step_data.data(), cfg.stride);
        break;
      case 16:
        writeIFFT<int16_t>(step_data.data(), cfg.stride);
        break;
      case 24:
        writeIFFT<int24_t>(step_data.data(), cfg.stride);
        break;
      case 32:
        writeIFFT<int32_t>(step_data.data(), cfg.stride);
        break;
      default:
        LOGE("Unsupported bits: %d", cfg.bits_per_sample);
    }
  }

  template <typename T>
  void writeIFFT(float *data, int len) {
    for (int j = 0; j < len; j++) {
      T sample = data[j];
      T out_data[cfg.channels];
      for (int ch = 0; ch < cfg.channels; ch++) {
        out_data[ch] = sample;
      }
      int result = rfft_data.writeArray((uint8_t *)out_data, sizeof(out_data));
      assert(result == sizeof(out_data));
    }
  }

  inline int bytesPerSample() { return cfg.bits_per_sample / 8; }

  /// make sure that we do not reuse already found results
  template <int N>
  void insertSorted(AudioFFTResult (&result)[N], AudioFFTResult tmp) {
    // find place where we need to insert new record
    for (int j = 0; j < N; j++) {
      // insert when biggen then current record
      if (tmp.magnitude > result[j].magnitude) {
        // shift existing values right
        for (int i = N - 2; i >= j; i--) {
          result[i + 1] = result[i];
        }
        // insert new value
        result[j] = tmp;
        // stop after we found the correct index
        break;
      }
    }
  }

  // adds samples to stride buffer, returns true if the buffer is full
  bool writeStrideBuffer(uint8_t *buffer, size_t len) {
    assert(stride_buffer.availableForWrite() >= len);
    stride_buffer.writeArray(buffer, len);
    return stride_buffer.isFull();
  }

  bool isPowerOfTwo(uint16_t x) { return (x & (x - 1)) == 0; }
};

}  // namespace audio_tools
