#pragma once

#include <TensorFlowLite.h>

#include <cmath>
#include <cstdint>

#include "AudioTools/AudioOutput.h"
#include "AudioTools/Buffers.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "tensorflow/lite/micro/micro_error_reporter.h"
//#include "tensorflow/lite/version.h"

namespace audio_tools {


// Forward Declarations
class TfLiteAudioFeatureProvider;


/**
 * @brief Error Reporter using the Audio Tools Logger
 * @author Phil Schatzmann
 * @copyright GPLv3 
 */
class TfLiteAudioErrorReporter : public tflite::ErrorReporter {
 public:
  virtual ~TfLiteAudioErrorReporter() {}
  virtual int Report(const char* format, va_list args) override {
    int result = snprintf(msg, 200, format, args);
    LOGE(msg);
    return result;
  }

 protected:
  char msg[200];
} my_error_reporter;
tflite::ErrorReporter* error_reporter = &my_error_reporter;


/**
 * @brief Configuration settings for TfLiteAudioOutput
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

struct TfLiteConfig {
    const unsigned char* model=nullptr;
    TfLiteAudioFeatureProvider* featureProvider=nullptr;
    const char** labels=nullptr;
    bool useAllOpsResolver=false;
    void (*respondToCommand)(const char* found_command, uint8_t score,
                           bool is_new_command) = nullptr;

  // Create an area of memory to use for input, output, and intermediate arrays.
  // The size of this will depend on the model you’re using, and may need to be
  // determined by experimentation.
    int kTensorArenaSize = 10 * 1024;

  // Keeping these as constant expressions allow us to allocate fixed-sized
  // arrays on the stack for our working memory.

  // The size of the input time series data we pass to the FFT to produce
  // the frequency information. This has to be a power of two, and since
  // we're dealing with 30ms of 16KHz inputs, which means 480 samples, this
  // is the next value.
  int kMaxAudioSampleSize = 480;
  int kAudioSampleFrequency = 16000;

  // Number of audio channels - is usually 1. If 2 we reduce it to 1 by averaging the 2 channels
  int kAudioChannels = 1;

  // The following values are derived from values used during model training.
  // If you change the way you preprocess the input, update all these constants.
  int kFeatureSliceSize = 40;
  int kFeatureSliceCount = 49;
  int kFeatureSliceStrideMs = 20;
  int kFeatureSliceDurationMs = 30;

  // number of new slices to collect before evaluating the model
  int kSlicesToProcess = 3;

  int featureElementCount() { return kFeatureSliceSize * kFeatureSliceCount; }

  // Parameters for RecognizeCommands
  int32_t average_window_duration_ms = 1000;
  uint8_t detection_threshold = 200;
  int32_t suppression_ms = 1500;
  int32_t minimum_count = 3;

};

// Partial implementation of std::dequeue, just providing the functionality
// that's needed to keep a record of previous neural network results over a
// short time period, so they can be averaged together to produce a more
// accurate overall prediction. This doesn't use any dynamic memory allocation
// so it's a better fit for microcontroller applications, but this does mean
// there are hard limits on the number of results it can store.
template <int N>
class TfLiteResultsQueue {
 public:
  TfLiteResultsQueue() : front_index_(0), size_(0) {}

  // Data structure that holds an inference result, and the time when it
  // was recorded.
  struct Result {
    Result() : time_(0), scores() {}
    Result(int32_t time, int8_t* input_scores) : time_(time) {
      for (int i = 0; i < N; ++i) {
        scores[i] = input_scores[i];
      }
    }
    int32_t time_;
    int8_t scores[N];
  };

  int size() { return size_; }
  bool empty() { return size_ == 0; }
  Result& front() { return results_[front_index_]; }
  Result& back() {
    int back_index = front_index_ + (size_ - 1);
    if (back_index >= kMaxResults) {
      back_index -= kMaxResults;
    }
    return results_[back_index];
  }

  void push_back(const Result& entry) {
    if (size() >= kMaxResults) {
      LOGE("Couldn't push_back latest result, too many already!");
      return;
    }
    size_ += 1;
    back() = entry;
  }

  Result pop_front() {
    if (size() <= 0) {
      LOGE("Couldn't pop_front result, none present!");
      return Result();
    }
    Result result = front();
    front_index_ += 1;
    if (front_index_ >= kMaxResults) {
      front_index_ = 0;
    }
    size_ -= 1;
    return result;
  }

  // Most of the functions are duplicates of dequeue containers, but this
  // is a helper that makes it easy to iterate through the contents of the
  // queue.
  Result& from_front(int offset) {
    if ((offset < 0) || (offset >= size_)) {
      LOGE("Attempt to read beyond the end of the queue!");
      offset = size_ - 1;
    }
    int index = front_index_ + offset;
    if (index >= kMaxResults) {
      index -= kMaxResults;
    }
    return results_[index];
  }

 private:
  static constexpr int kMaxResults = 50;
  Result results_[kMaxResults];

  int front_index_;
  int size_;
};

/**
 * @brief Base class for implementing different primitive decoding models on top of the
 * instantaneous results from running an audio recognition model on a single
 * window of samples.
 * 
 * @tparam N 
 */
template <int N>
class TfLiteAbstractRecognizeCommands {
 public:
  virtual TfLiteStatus ProcessLatestResults(const TfLiteTensor* latest_results,
                                    const int32_t current_time_ms,
                                    const char** found_command, uint8_t* score,
                                    bool* is_new_command)  = 0;

  virtual bool begin(TfLiteConfig cfg) = 0;
};

/**
 * @brief This class is designed to apply a very primitive decoding model on top of the
 * instantaneous results from running an audio recognition model on a single
 * window of samples. It applies smoothing over time so that noisy individual
 * label scores are averaged, increasing the confidence that apparent matches
 * are real.
 * To use it, you should create a class object with the configuration you
 * want, and then feed results from running a TensorFlow model into the
 * processing method. The timestamp for each subsequent call should be
 * increasing from the previous, since the class is designed to process a stream
 * of data over time.
 */

template <int N>
class TfLiteRecognizeCommands : public TfLiteAbstractRecognizeCommands<N> {
 public:
  // labels should be a list of the strings associated with each one-hot score.
  // The window duration controls the smoothing. Longer durations will give a
  // higher confidence that the results are correct, but may miss some commands.
  // The detection threshold has a similar effect, with high values increasing
  // the precision at the cost of recall. The minimum count controls how many
  // results need to be in the averaging window before it's seen as a reliable
  // average. This prevents erroneous results when the averaging window is
  // initially being populated for example. The suppression argument disables
  // further recognitions for a set time after one has been triggered, which can
  // help reduce spurious recognitions.

  explicit TfLiteRecognizeCommands() {
    previous_top_label_ = "silence";
    previous_top_label_time_ = std::numeric_limits<int32_t>::min();
    kCategoryCount = N;
  }

  /// Setup parameters from config
  bool begin(TfLiteConfig cfg) override {
    average_window_duration_ms_ = cfg.average_window_duration_ms;
    detection_threshold_ = cfg.detection_threshold;
    suppression_ms_ = cfg.suppression_ms;
    minimum_count_ = cfg.minimum_count;
    kCategoryLabels = cfg.labels;
    if (cfg.labels==0){
      LOGW("config.labels not defined");
      return false;
    }
    return true;
  }

  // Call this with the results of running a model on sample data.
  virtual TfLiteStatus ProcessLatestResults(const TfLiteTensor* latest_results,
                                    const int32_t current_time_ms,
                                    const char** found_command, uint8_t* score,
                                    bool* is_new_command) override {
    LOGD(LOG_METHOD);
    if ((latest_results->dims->size != 2) ||
        (latest_results->dims->data[0] != 1) ||
        (latest_results->dims->data[1] != kCategoryCount)) {
      LOGE(
          "The results for recognition should contain %d "
          "elements, but there are "
          "%d in an %d-dimensional shape",
          kCategoryCount, (int)latest_results->dims->data[1],
          (int)latest_results->dims->size);
      return kTfLiteError;
    }

    if (latest_results->type != kTfLiteInt8) {
      LOGE("The results for recognition should be int8 elements, but are %d",
           (int)latest_results->type);
      return kTfLiteError;
    }

    if ((!previous_results_.empty()) &&
        (current_time_ms < previous_results_.front().time_)) {
      LOGE("Results must be in increasing time order: timestamp  %d < %d",
           (int)current_time_ms, (int)previous_results_.front().time_);
      return kTfLiteError;
    }

    // Add the latest results to the head of the queue.
    previous_results_.push_back({current_time_ms, latest_results->data.int8});

    // Prune any earlier results that are too old for the averaging window.
    const int64_t time_limit = current_time_ms - average_window_duration_ms_;
    while ((!previous_results_.empty()) &&
           previous_results_.front().time_ < time_limit) {
      previous_results_.pop_front();
    }

    // If there are too few results, assume the result will be unreliable
    // and bail.
    const int64_t how_many_results = previous_results_.size();
    const int64_t earliest_time = previous_results_.front().time_;
    const int64_t samples_duration = current_time_ms - earliest_time;
    if ((how_many_results < minimum_count_) ||
        (samples_duration < (average_window_duration_ms_ / 4))) {
      *found_command = previous_top_label_;
      *score = 0;
      *is_new_command = false;
      return kTfLiteOk;
    }

    // Calculate the average score across all the results in the window.
    int32_t average_scores[kCategoryCount];
    for (int offset = 0; offset < previous_results_.size(); ++offset) {
      auto previous_result = previous_results_.from_front(offset);
      const int8_t* scores = previous_result.scores;
      for (int i = 0; i < kCategoryCount; ++i) {
        if (offset == 0) {
          average_scores[i] = scores[i] + 128;
        } else {
          average_scores[i] += scores[i] + 128;
        }
      }
    }
    for (int i = 0; i < kCategoryCount; ++i) {
      average_scores[i] /= how_many_results;
    }

    // Find the current highest scoring category.
    int current_top_index = 0;
    int32_t current_top_score = 0;
    for (int i = 0; i < kCategoryCount; ++i) {
      if (average_scores[i] > current_top_score) {
        current_top_score = average_scores[i];
        current_top_index = i;
      }
    }
    const char* current_top_label = kCategoryLabels[current_top_index];

    // If we've recently had another label trigger, assume one that occurs
    // too soon afterwards is a bad result.
    int64_t time_since_last_top;
    if ((previous_top_label_ == kCategoryLabels[0]) ||
        (previous_top_label_time_ == std::numeric_limits<int32_t>::min())) {
      time_since_last_top = std::numeric_limits<int32_t>::max();
    } else {
      time_since_last_top = current_time_ms - previous_top_label_time_;
    }
    if ((current_top_score > detection_threshold_) &&
        ((current_top_label != previous_top_label_) ||
         (time_since_last_top > suppression_ms_))) {
      previous_top_label_ = current_top_label;
      previous_top_label_time_ = current_time_ms;
      *is_new_command = true;
    } else {
      *is_new_command = false;
    }
    *found_command = current_top_label;
    *score = current_top_score;

    return kTfLiteOk;
  }

 protected:
  // Configuration
  int32_t average_window_duration_ms_;
  uint8_t detection_threshold_;
  int32_t suppression_ms_;
  int32_t minimum_count_;
  int kCategoryCount;
  const char** kCategoryLabels = nullptr;

  // Working variables
  TfLiteResultsQueue<N> previous_results_;
  const char* previous_top_label_;
  int32_t previous_top_label_time_;
};


/**
 * @brief FeatureProvider for Audio Data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteAudioFeatureProvider {
 public:

  TfLiteAudioFeatureProvider() = default;

  ~TfLiteAudioFeatureProvider() {
    if (p_buffer != nullptr) delete p_buffer;
  }

  /// Call begin before starting the processing
  virtual bool begin(TfLiteConfig config) {
    LOGD(LOG_METHOD);
    cfg = config;
    if (p_buffer == nullptr) {
      p_buffer = new audio_tools::RingBuffer<int16_t>(cfg.kMaxAudioSampleSize);
      LOGD("Allocating buffer for %d samples", cfg.kMaxAudioSampleSize);
    }
    // Initialize the feature data to default values.
    if (feature_data_ == nullptr) {
      feature_data_ = new int8_t[cfg.featureElementCount()]{};  // initialzed array
    }

    TfLiteStatus init_status = initializeMicroFeatures();
    if (init_status != kTfLiteOk) {
      return false;
    }
    return true;
  }

  // Fills the feature data with information from audio inputs, and returns how
  // many feature slices were updated.
  virtual int write(const uint8_t* audio, size_t bytes) {
    LOGD("write: %u", (unsigned)bytes);
    uint16_t slice_count = 0;
    int16_t* audio_16 = (int16_t*)audio;
    // process all samples
    for (int j = 0; j < bytes / 2; j += cfg.kAudioChannels) {
      // if buffer is full we create a new slice
      if (p_buffer->availableForWrite() == 0) {
        addSlice();
        slice_count++;
      }

      // add values to buffer converting from int16_t to int8_t
      if (cfg.kAudioChannels == 1) {
        p_buffer->write(audio_16[j]);
      } else {
        // calculate avg of 2 channels and convert it to int8_t
        p_buffer->write(((audio_16[j] / 2) + (audio_16[j + 1] / 2)));
      }
    }
    return slice_count;
  }

 protected:
  TfLiteConfig cfg;
  // int feature_size_;
  int8_t* feature_data_ = nullptr;
  // Make sure we don't try to use cached information if this is the first
  // call into the provider.
  bool is_first_run_ = true;
  bool g_is_first_time = true;
  //  const char** kCategoryLabels;
  audio_tools::RingBuffer<int16_t>* p_buffer = nullptr;
  FrontendState g_micro_features_state;

  // If we can avoid recalculating some slices, just move the existing
  // data up in the spectrogram, to perform something like this: last time
  // = 80ms          current time = 120ms
  // +-----------+             +-----------+
  // | data@20ms |         --> | data@60ms |
  // +-----------+       --    +-----------+
  // | data@40ms |     --  --> | data@80ms |
  // +-----------+   --  --    +-----------+
  // | data@60ms | --  --      |  <empty>  |
  // +-----------+   --        +-----------+
  // | data@80ms | --          |  <empty>  |
  // +-----------+             +-----------+
  virtual void addSlice() {
    LOGD(LOG_METHOD);
    memmove(feature_data_, feature_data_ + cfg.kFeatureSliceSize,
            (cfg.kFeatureSliceCount - 1) * cfg.kFeatureSliceSize);

    // copy data from buffer to audio_samples
    int16_t audio_samples[cfg.kMaxAudioSampleSize];
    int audio_samples_size =
        p_buffer->readArray(audio_samples, cfg.kMaxAudioSampleSize);


    //  the new slice data will always be stored at the end
    int8_t* new_slice_data =
        feature_data_ + ((cfg.kFeatureSliceCount - 1) * cfg.kFeatureSliceSize);
    size_t num_samples_read = audio_samples_size;
    if (generateMicroFeatures(audio_samples, audio_samples_size,
                              cfg.kFeatureSliceSize, new_slice_data,
                              &num_samples_read) != kTfLiteOk) {
      LOGE("Error generateMicroFeatures");
    }

    // printFeatures();
  }

  /// For debugging: print feature matrix
  void printFeatures() {
    for (int i = 0; i < cfg.kFeatureSliceCount; i++) {
      for (int j = 0; j < cfg.kFeatureSliceSize; j++) {
        Serial.print(feature_data_[(i * cfg.kFeatureSliceSize) + j]);
        Serial.print(" ");
      }
      Serial.println();
    }
  }

  virtual TfLiteStatus initializeMicroFeatures() {
    LOGD(LOG_METHOD);
    FrontendConfig config;
    config.window.size_ms = cfg.kFeatureSliceDurationMs;
    config.window.step_size_ms = cfg.kFeatureSliceStrideMs;
    config.noise_reduction.smoothing_bits = 10;
    config.filterbank.num_channels = cfg.kFeatureSliceSize;
    config.filterbank.lower_band_limit = 125.0;
    config.filterbank.upper_band_limit = 7500.0;
    config.noise_reduction.smoothing_bits = 10;
    config.noise_reduction.even_smoothing = 0.025;
    config.noise_reduction.odd_smoothing = 0.06;
    config.noise_reduction.min_signal_remaining = 0.05;
    config.pcan_gain_control.enable_pcan = 1;
    config.pcan_gain_control.strength = 0.95;
    config.pcan_gain_control.offset = 80.0;
    config.pcan_gain_control.gain_bits = 21;
    config.log_scale.enable_log = 1;
    config.log_scale.scale_shift = 6;
    if (!FrontendPopulateState(&config, &g_micro_features_state,
                               cfg.kAudioSampleFrequency)) {
      LOGE("FrontendPopulateState() failed");
      return kTfLiteError;
    }
    g_is_first_time = true;
    return kTfLiteOk;
  }

  // This is not exposed in any header, and is only used for testing, to ensure
  // that the state is correctly set up before generating results.
  void setMicroFeaturesNoiseEstimates(const uint32_t* estimate_presets) {
    LOGD(LOG_METHOD);
    for (int i = 0; i < g_micro_features_state.filterbank.num_channels; ++i) {
      g_micro_features_state.noise_reduction.estimate[i] = estimate_presets[i];
    }
  }

  virtual TfLiteStatus generateMicroFeatures(const int16_t* input, int input_size,
                                     int output_size, int8_t* output,
                                     size_t* num_samples_read) {
    LOGD(LOG_METHOD);
    const int16_t* frontend_input;
    if (g_is_first_time) {
      frontend_input = input;
      g_is_first_time = false;
    } else {
      frontend_input = input;
    }

    // Apply FFT
    FrontendOutput frontend_output = FrontendProcessSamples(
        &g_micro_features_state, frontend_input, input_size, num_samples_read);

    for (size_t i = 0; i < frontend_output.size; ++i) {
      // These scaling values are derived from those used in input_data.py in
      // the training pipeline. The feature pipeline outputs 16-bit signed
      // integers in roughly a 0 to 670 range. In training, these are then
      // arbitrarily divided by 25.6 to get float values in the rough range of
      // 0.0 to 26.0. This scaling is performed for historical reasons, to match
      // up with the output of other feature generators. The process is then
      // further complicated when we quantize the model. This means we have to
      // scale the 0.0 to 26.0 real values to the -128 to 127 signed integer
      // numbers. All this means that to get matching values from our integer
      // feature output into the tensor input, we have to perform: input =
      // (((feature / 25.6) / 26.0) * 256) - 128 To simplify this and perform it
      // in 32-bit integer math, we rearrange to: input = (feature * 256) /
      // (25.6 * 26.0) - 128
      constexpr int32_t value_scale = 256;
      constexpr int32_t value_div =
          static_cast<int32_t>((25.6f * 26.0f) + 0.5f);
      int32_t value =
          ((frontend_output.values[i] * value_scale) + (value_div / 2)) /
          value_div;
      value -= 128;
      if (value < -128) {
        value = -128;
      }
      if (value > 127) {
        value = 127;
      }
      output[i] = value;
    }

    return kTfLiteOk;
  }
};



/**
 * @brief TfLiteAudioOutput which uses Tensorflow Light to analyze the data
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <int N>
class TfLiteAudioOutput : public AudioPrint {
 public:
  TfLiteAudioOutput() {}

  /// Optionally define your own recognizer
  void setRecognizer(TfLiteAbstractRecognizeCommands<N>* p_recognizer) {
    recognizer = p_recognizer;
  }

  /// Optionally define your own interpreter
  void setInterpreter(tflite::MicroInterpreter* p_interpreter) {
    this->interpreter = p_interpreter;
  }

  // Provides the default configuration
  virtual TfLiteConfig defaultConfig() {
     TfLiteConfig def;
     return def;
  }

  /// Start the processing
  virtual bool begin(TfLiteConfig config) {
    LOGD(LOG_METHOD);
    cfg = config;

    // alloatme memory
    tensor_arena = new uint8_t[cfg.kTensorArenaSize];
    feature_buffer = new int8_t[cfg.featureElementCount()];

    if (!setupRecognizer()){
      LOGE("setupRecognizer");
      return false;
    }

    // setup the feature provider
    if (!setupFeatureProvider()){
      LOGE("setupFeatureProvider");
      return false;
    }

    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    if (!setModel(cfg.model)) {
      return false;
    }

    if (!setupInterpreter()) {
      return false;
    }

    // Allocate memory from the tensor_arena for the model's tensors.
    LOGI("AllocateTensors");
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
      LOGE("AllocateTensors() failed");
      return false;
    }

    // Get information about the memory area to use for the model's input.
    LOGI("Get Input");
    model_input = interpreter->input(0);
    if ((model_input->dims->size != 2) || (model_input->dims->data[0] != 1) ||
        (model_input->dims->data[1] != (cfg.kFeatureSliceCount *
                                        cfg.kFeatureSliceSize)) ||
        (model_input->type != kTfLiteInt8)) {
      LOGE("Bad input tensor parameters in model");
      return false;
    }

    LOGI("Get Buffer");
    model_input_buffer = model_input->data.int8;
    if (model_input_buffer == nullptr) {
      LOGE("model_input_buffer is null");
      return false;
    }

    // all good if we made it here
    is_setup = true;
    LOGI("done");
    return true;
  }

  /// Constant streaming
  int availableForWrite() { return DEFAULT_BUFFER_SIZE; }

  /// process the data in batches of max kMaxAudioSampleSize.
  size_t write(const uint8_t* audio, size_t bytes) {
    LOGD(LOG_METHOD);
    if (!is_setup) return 0;
    size_t result = 0;
    int pos = 0;
    int open = bytes;
    // we submit int16 data which will be reduced to 8bits so we can send
    // double the amount - 2 channels will be recuced to 1 so we multiply by
    // number of channels
    int maxBytes = cfg.kMaxAudioSampleSize * 2 * cfg.kAudioChannels;
    while (open > 0) {
      int len = min(open, maxBytes);
      result += processAudio(audio + pos, len);
      open -= len;
      pos += len;
    }
    return result;
  }

 protected:
  const tflite::Model* p_model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* model_input = nullptr;
  TfLiteAudioFeatureProvider* feature_provider = nullptr;
  TfLiteAbstractRecognizeCommands<N>* recognizer = nullptr;
  int32_t current_time = 0;
  int16_t total_slice_count = 0;
  bool is_setup = false;
  TfLiteConfig cfg;

  // Create an area of memory to use for input, output, and intermediate
  // arrays. The size of this will depend on the model you're using, and may
  // need to be determined by experimentation.
  uint8_t* tensor_arena = nullptr;
  int8_t* feature_buffer = nullptr;
  int8_t* model_input_buffer = nullptr;

  bool setModel(const unsigned char* model) {
    LOGD(LOG_METHOD);
    p_model = tflite::GetModel(model);
    if (p_model->version() != TFLITE_SCHEMA_VERSION) {
      LOGE(
          "Model provided is schema version %d not equal "
          "to supported version %d.",
          p_model->version(), TFLITE_SCHEMA_VERSION);
      return false;
    }
    return true;
  }

  bool setupRecognizer() {
    // setup default recognizer if not defined
    if (recognizer == nullptr) {
      static TfLiteRecognizeCommands<N> static_recognizer;
      recognizer = &static_recognizer;
    }
    return recognizer->begin(cfg);
  }

  bool setupFeatureProvider() {
    if (cfg.featureProvider!=nullptr){
      feature_provider = cfg.featureProvider;
    } else {
      static TfLiteAudioFeatureProvider featureProvider;
      feature_provider = &featureProvider;
    }
    return feature_provider->begin(cfg);
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  bool setupInterpreter() {
    if (interpreter == nullptr) {
      LOGI(LOG_METHOD);
      if (cfg.useAllOpsResolver){
        tflite::AllOpsResolver resolver;
        static tflite::MicroInterpreter static_interpreter(
            p_model, resolver, tensor_arena, cfg.kTensorArenaSize,
            error_reporter);
        interpreter = &static_interpreter;
      } else {
        // NOLINTNEXTLINE(runtime-global-variables)
        static tflite::MicroMutableOpResolver<4> micro_op_resolver(error_reporter);
        if (micro_op_resolver.AddDepthwiseConv2D() != kTfLiteOk) {
          return false;
        }
        if (micro_op_resolver.AddFullyConnected() != kTfLiteOk) {
          return false;
        }
        if (micro_op_resolver.AddSoftmax() != kTfLiteOk) {
          return false;
        }
        if (micro_op_resolver.AddReshape() != kTfLiteOk) {
          return false;
        }
        // Build an interpreter to run the model with.
        static tflite::MicroInterpreter static_interpreter(
            p_model, micro_op_resolver, tensor_arena, cfg.kTensorArenaSize,
            error_reporter);
        interpreter = &static_interpreter;
      }
    }
    return true;
  }

  // Process a slince of audio data - returns the number of bytes
  size_t processAudio(const uint8_t* audio, size_t bytes) {
    LOGD("process: %u", (unsigned)bytes);
    // Update the spectrogram
    total_slice_count += feature_provider->write(audio, bytes);

    // run network model only if we have the necessary slices
    if (total_slice_count < cfg.kSlicesToProcess) {
      return bytes;
    }

    LOGI("->slices: %d", total_slice_count);
    // Copy feature buffer to input tensor
    // for (int i = 0; i < cfg.featureElementCount(); i++) {
    //   model_input_buffer[i] = feature_buffer[i];
    // }
    memcpy(model_input_buffer, feature_buffer, cfg.featureElementCount());

    // Run the model on the spectrogram input and make sure it succeeds.
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      LOGE("Invoke failed");
      return 0;
    }

    // Obtain a pointer to the output tensor
    TfLiteTensor* output = interpreter->output(0);

    // determine time
    current_time += cfg.kFeatureSliceStrideMs * total_slice_count;
    // Determine whether a command was recognized 
    const char* found_command = nullptr;
    uint8_t score = 0;
    bool is_new_command = false;

    TfLiteStatus process_status = recognizer->ProcessLatestResults(
        output, current_time, &found_command, &score, &is_new_command);
    if (process_status != kTfLiteOk) {
      LOGE("TfLiteRecognizeCommands::ProcessLatestResults() failed");
      return 0;
    }
    // Do something based on the recognized command. The default
    // implementation just prints to the error console, but you should replace
    // this with your own function for a real application.
    respondToCommand(found_command, score, is_new_command);

    // reset total_slice_count
    total_slice_count = 0;

    // all bytes processed successfully
    return bytes;
  }

  /// Overwrite this method to implement your own handler or provide callback
  virtual void respondToCommand(const char* found_command, uint8_t score,
                                bool is_new_command) {
    if (cfg.respondToCommand != nullptr) {
      cfg.respondToCommand(found_command, score, is_new_command);
    } else {
      LOGD(LOG_METHOD);
      if (is_new_command) {
        char buffer[80];
        sprintf(buffer, "Result: %s, score: %d, is_new: %s", found_command,
                score, is_new_command ? "true" : "false");
        Serial.println(buffer);
      }
    }
  };
};

} // namespace