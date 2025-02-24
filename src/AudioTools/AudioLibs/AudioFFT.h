#pragma once

#include "AudioTools/AudioLibs/FFT/FFTWindows.h"
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
  int bin;
  float magnitude;
  float frequency;

  int frequencyAsInt() { return round(frequency); }
  const char *frequencyAsNote() { return AudioFFTNotes.note(frequency); }
  const char *frequencyAsNote(float &diff) {
    return AudioFFTNotes.note(frequency, diff);
  }
};

/**
 * @brief Configuration for AudioFFT. If there are more then 1 channel the
 * channel_used is defining which channel is used to perform the fft on.
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
  void* ref = nullptr;
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
  void resize(int size) {
    // reset max for new scaling
    rfft_max = 0.0;
    // define new size
    len = size;
    data.resize(size);
    for (int j = 0; j < data.size(); j++) {
      data[j] = 0.0;
    }
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
  AudioFFTBase(FFTDriver *driver) { p_driver = driver; }

  ~AudioFFTBase() { end(); }

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
    if (cfg.window_function_fft==nullptr) cfg.window_function_fft = cfg.window_function;
    if (cfg.window_function_ifft==nullptr) cfg.window_function_ifft = cfg.window_function;
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
    if (cfg.window_function_ifft != nullptr 
    && cfg.window_function_ifft != cfg.window_function_fft) {
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

    if (!is_valid_rxtx){
      LOGE("Invalid rxtx_mode");
      return false;
    }

    current_pos = 0;
    return p_driver->isValid();
  }

  /// Just resets the current_pos e.g. to start a new cycle
  void reset() {
    current_pos = 0;
    if (cfg.window_function_fft != nullptr) {
      cfg.window_function_fft->begin(cfg.length);
    }
    if (cfg.window_function_ifft != nullptr) {
      cfg.window_function_ifft->begin(cfg.length);
    }
  }

  operator bool() { return p_driver != nullptr && p_driver->isValid(); }

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
    if (cfg.rxtx_mode == RX_MODE && cfg.callback != nullptr && rfft_data.available() == 0) {
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
    ret_value.magnitude = 0;
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
  int frequencyToBin(int freq){
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
  float phase(int bin){
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
  bool setBin(int pos, FFTBin &bin) {
    return setBin(pos, bin.real, bin.img);
  }
  /// gets the value of a bin
  bool getBin(int pos, FFTBin &bin) { return p_driver->getBin(pos, bin); }

  /// clears the fft data
  void clearBins(){
    FFTBin empty{0,0};
    for (int j=0; j< size(); j++){
      setBin(j, empty);
    }  
  }

  /// Provides the actual configuration
  AudioFFTConfig &config() { return cfg; }

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
  SingleBuffer<uint8_t> stride_buffer{0};
  RingBuffer<uint8_t> rfft_data{0};
  bool has_rfft_data = false;

  // Add samples to input data p_x - and process them if full
  template <typename T>
  void processSamples(const void *data, size_t count) {
    T *dataT = (T *)data;
    T sample;
    for (int j = 0; j < count; j += cfg.channels) {
      sample = dataT[j + cfg.channel_used];
      if (writeStrideBuffer((uint8_t *)&sample, sizeof(T))){
        // process data if buffer is full
        T* samples = (T*) stride_buffer.data();
        int sample_count = stride_buffer.size() / sizeof(T);
        assert(sample_count == cfg.length);
        for (int j=0; j< sample_count; j++){
          T out_sample = samples[j];
          p_driver->setValue(j, windowedSample(out_sample, j));
        }

        fft<T>();

        // remove stride samples
        stride_buffer.clearArray(cfg.stride * sizeof(T));

        // validate available data in stride buffer
        if (cfg.stride == cfg.length) assert(stride_buffer.available()==0);

      }
    }
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
