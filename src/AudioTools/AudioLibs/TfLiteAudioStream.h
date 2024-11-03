#pragma once

// Configure FFT to output 16 bit fixed point.
#define FIXED_POINT 16

//#include <MicroTFLite.h>
#include <TensorFlowLite.h>
#include <cmath>
#include <cstdint>
#include "AudioTools/CoreAudio/AudioOutput.h"
#include "AudioTools/CoreAudio/Buffers.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

/** 
 * @defgroup tflite TFLite
 * @ingroup ml
 * @brief Tensorflow 
**/


namespace audio_tools {

// Forward Declarations
class TfLiteAudioStreamBase;
class TfLiteAbstractRecognizeCommands;

/**
 * @brief Input class which provides the next value if the TfLiteAudioStream is treated as an audio sourcce
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteReader {
  public:
    virtual bool begin(TfLiteAudioStreamBase *parent) = 0;
    virtual int read(int16_t*data, int len) = 0;
};

/**
 * @brief Output class which interprets audio data if TfLiteAudioStream is treated as audio sink
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteWriter {
  public:
    virtual bool begin(TfLiteAudioStreamBase *parent) = 0;
    virtual bool write(const int16_t sample) = 0;
};

/**
 * @brief Configuration settings for TfLiteAudioStream
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

struct TfLiteConfig {
  friend class TfLiteMicroSpeechRecognizeCommands;
  const unsigned char* model = nullptr;
  TfLiteReader *reader = nullptr;
  TfLiteWriter *writer = nullptr;
  TfLiteAbstractRecognizeCommands *recognizeCommands=nullptr;
  bool useAllOpsResolver = false;
  // callback for command handler
  void (*respondToCommand)(const char* found_command, uint8_t score,
                           bool is_new_command) = nullptr;

  // Create an area of memory to use for input, output, and intermediate arrays.
  // The size of this will depend on the model you’re using, and may need to be
  // determined by experimentation.
  size_t kTensorArenaSize = 10 * 1024;

  // Keeping these as constant expressions allow us to allocate fixed-sized
  // arrays on the stack for our working memory.

  // The size of the input time series data we pass to the FFT to produce
  // the frequency information. This has to be a power of two, and since
  // we're dealing with 30ms of 16KHz inputs, which means 480 samples, this
  // is the next value.
  // int kMaxAudioSampleSize =  320; //512; // 480
  int sample_rate = 16000;

  // Number of audio channels - is usually 1. If 2 we reduce it to 1 by
  // averaging the 2 channels
  int channels = 1;

  // The following values are derived from values used during model training.
  // If you change the way you preprocess the input, update all these constants.
  int kFeatureSliceSize = 40;
  int kFeatureSliceCount = 49;
  int kFeatureSliceStrideMs = 20;
  int kFeatureSliceDurationMs = 30;

  // number of new slices to collect before evaluating the model
  int kSlicesToProcess = 2;

  // Parameters for RecognizeCommands
  int32_t average_window_duration_ms = 1000;
  uint8_t detection_threshold = 50;
  int32_t suppression_ms = 1500;
  int32_t minimum_count = 3;

  // input for FrontendConfig
  float filterbank_lower_band_limit = 125.0;
  float filterbank_upper_band_limit = 7500.0;
  float noise_reduction_smoothing_bits = 10;
  float noise_reduction_even_smoothing = 0.025;
  float noise_reduction_odd_smoothing = 0.06;
  float noise_reduction_min_signal_remaining = 0.05;
  bool pcan_gain_control_enable_pcan = 1;
  float pcan_gain_control_strength = 0.95;
  float pcan_gain_control_offset = 80.0;
  float pcan_gain_control_gain_bits = 21;
  bool log_scale_enable_log = 1;
  uint8_t log_scale_scale_shift = 6;

  /// Defines the labels
  template<int N>
  void setCategories(const char* (&array)[N]){
    labels = array;
    kCategoryCount = N;
  }
 
  int categoryCount() {
    return kCategoryCount;
  }

  int featureElementCount() { 
    return kFeatureSliceSize * kFeatureSliceCount;
  }

  int audioSampleSize() {
    return kFeatureSliceDurationMs * (sample_rate / 1000);
  }

  int strideSampleSize() {
    return kFeatureSliceStrideMs * (sample_rate / 1000);
  }

  private:
    int  kCategoryCount = 0;
    const char** labels = nullptr;
};

/**
 * @brief Quantizer that helps to quantize and dequantize between float and int8
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteQuantizer {
  public:
    // convert float to int8
    static int8_t quantize(float value, float scale, float zero_point){
      if(scale==0.0&&zero_point==0) return value;
      return value / scale + zero_point;
    }
    // convert int8 to float
    static float dequantize(int8_t value, float scale, float zero_point){
      if(scale==0.0&&zero_point==0) return value;
      return (value - zero_point) * scale;
    }

    static float dequantizeToNewRange(int8_t value, float scale, float zero_point, float new_range){
      float deq = (static_cast<float>(value) - zero_point) * scale;
      return clip(deq * new_range, new_range);
    }

    static float clip(float value, float range){
      if (value>=0.0){
        return value > range ? range : value;
      } else {
        return -value < -range ? -range : value;
      }
    }
};

/**
 * @brief Base class for implementing different primitive decoding models on top
 * of the instantaneous results from running an audio recognition model on a
 * single window of samples.
 * @ingroup tflite
 */
class TfLiteAbstractRecognizeCommands {
 public:
  virtual bool begin(TfLiteConfig cfg) = 0;
  virtual TfLiteStatus getCommand(const TfLiteTensor* latest_results, const int32_t current_time_ms,
                                  const char** found_command,uint8_t* score,bool* is_new_command) = 0;

};

/**
 * @brief This class is designed to apply a very primitive decoding model on top
 * of the instantaneous results from running an audio recognition model on a
 * single window of samples. It applies smoothing over time so that noisy
 * individual label scores are averaged, increasing the confidence that apparent
 * matches are real. To use it, you should create a class object with the
 * configuration you want, and then feed results from running a TensorFlow model
 * into the processing method. The timestamp for each subsequent call should be
 * increasing from the previous, since the class is designed to process a stream
 * of data over time.
 * @ingroup tflite
 */

class TfLiteMicroSpeechRecognizeCommands : public TfLiteAbstractRecognizeCommands {
 public:

  TfLiteMicroSpeechRecognizeCommands() {
  }

  /// Setup parameters from config
  bool begin(TfLiteConfig cfg) override {
    TRACED();
    this->cfg = cfg;
    if (cfg.labels == nullptr) {
      LOGE("config.labels not defined");
      return false;
    }
    return true;
  }

  // Call this with the results of running a model on sample data.
  virtual TfLiteStatus getCommand(const TfLiteTensor* latest_results,
                                            const int32_t current_time_ms,
                                            const char** found_command,
                                            uint8_t* score,
                                            bool* is_new_command) override {
                                            
    TRACED();
    this->current_time_ms = current_time_ms;
    this->time_since_last_top =  current_time_ms - previous_time_ms;

    deleteOldRecords(current_time_ms - cfg.average_window_duration_ms);
    int idx = resultCategoryIdx(latest_results->data.int8);
    Result row(current_time_ms, idx, latest_results->data.int8[idx]);
    result_queue.push_back(row);

    TfLiteStatus result = validate(latest_results);
    if (result!=kTfLiteOk){
      return result;
    }
    return evaluate(found_command, score, is_new_command);
  }

  protected:
      struct Result {
        int32_t time_ms;
        int category=0;
        int8_t score=0;

        Result() = default;
        Result(int32_t time_ms,int category, int8_t score){
          this->time_ms = time_ms;
          this->category = category;
          this->score = score;
        }
      };

      TfLiteConfig cfg;
      Vector <Result> result_queue;
      int previous_cateogory=-1;
      int32_t current_time_ms=0;
      int32_t previous_time_ms=0;
      int32_t time_since_last_top=0;

      /// finds the category with the biggest score
      int resultCategoryIdx(int8_t* score) {
        int result = -1;
        uint8_t top_score = std::numeric_limits<uint8_t>::min();
        for (int j=0;j<categoryCount();j++){
          if (score[j]>top_score){
            result = j;
          }
        }
        return result;
      }

      /// Determines the number of categories
      int categoryCount() {
        return cfg.categoryCount();
      }

      /// Removes obsolete records from the queue
      void deleteOldRecords(int32_t limit) {
        if (result_queue.empty()) return;
        while (result_queue[0].time_ms<limit){
          result_queue.pop_front();
        }
      }

      /// Finds the result
      TfLiteStatus evaluate(const char** found_command, uint8_t* result_score, bool* is_new_command) {
        TRACED();
        float totals[categoryCount()]={0};
        int count[categoryCount()]={0};
        // calculate totals
        for (int j=0;j<result_queue.size();j++){
          int idx = result_queue[j].category;
          totals[idx] += result_queue[j].score;
          count[idx]++;
        }

        // find max
        int maxIdx = -1;
        float max = -100000;
        for (int j=0;j<categoryCount();j++){
          if (totals[j]>max){
            max = totals[j];
            maxIdx = j;
          }
        }    

        if (maxIdx==-1){
          LOGE("Could not find max category")
          return kTfLiteError;
        }

        // determine result
        *result_score =  totals[maxIdx] / count[maxIdx];
        *found_command = cfg.labels[maxIdx];

        if (previous_cateogory!=maxIdx
        && *result_score > cfg.detection_threshold
        && time_since_last_top > cfg.suppression_ms){
          previous_time_ms = current_time_ms;
          previous_cateogory = maxIdx;
          *is_new_command = true;
        } else {
          *is_new_command = false;
        }

        LOGD("Category: %s, score: %d, is_new: %d",*found_command, *result_score, *is_new_command);

        return kTfLiteOk;
      }

      /// Checks the input data
      TfLiteStatus validate(const TfLiteTensor* latest_results) {
          if ((latest_results->dims->size != 2) ||
              (latest_results->dims->data[0] != 1) ||
              (latest_results->dims->data[1] != categoryCount())) {
            LOGE(
                "The results for recognition should contain %d "
                "elements, but there are "
                "%d in an %d-dimensional shape",
                categoryCount(), (int)latest_results->dims->data[1],
                (int)latest_results->dims->size);
            return kTfLiteError;
          }

          if (latest_results->type != kTfLiteInt8) {
            LOGE("The results for recognition should be int8 elements, but are %d",
                (int)latest_results->type);
            return kTfLiteError;
          }

          if ((!result_queue.empty()) &&
              (current_time_ms < result_queue[0].time_ms)) {
            LOGE("Results must be in increasing time order: timestamp  %d < %d",
                (int)current_time_ms, (int)result_queue[0].time_ms);
            return kTfLiteError;
          }
          return kTfLiteOk;
      }

};


/**
 * @brief Astract TfLiteAudioStream to provide access to TfLiteAudioStream for
 * Reader and Writers
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteAudioStreamBase : public AudioStream {
  public:
    virtual void setInterpreter(tflite::MicroInterpreter* p_interpreter) = 0;
    virtual TfLiteConfig defaultConfig() = 0;
    virtual bool begin(TfLiteConfig config) = 0;
    virtual int availableToWrite() = 0;

    /// process the data in batches of max kMaxAudioSampleSize.
    virtual size_t write(const uint8_t* data, size_t len)= 0;
    virtual tflite::MicroInterpreter& interpreter()= 0;

    /// Provides the TfLiteConfig information
    virtual TfLiteConfig &config()= 0;

    /// Provides access to the model input buffer
    virtual int8_t*  modelInputBuffer()= 0;
};

/**
 * @brief TfLiteMicroSpeachWriter for Audio Data
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteMicroSpeachWriter : public TfLiteWriter {
 public:
  TfLiteMicroSpeachWriter() = default;

  ~TfLiteMicroSpeachWriter() {
    if (p_buffer != nullptr) delete p_buffer;
    if (p_audio_samples != nullptr) delete p_audio_samples;
  }

  /// Call begin before starting the processing
  virtual bool begin(TfLiteAudioStreamBase *parent) {
    TRACED();
    this->parent = parent;
    cfg = parent->config();
    current_time = 0;
    kMaxAudioSampleSize = cfg.audioSampleSize();
    kStrideSampleSize = cfg.strideSampleSize();
    kKeepSampleSize = kMaxAudioSampleSize - kStrideSampleSize;

    if (!setup_recognizer()) {
      LOGE("setup_recognizer");
      return false;
    }

    // setup FrontendConfig
    TfLiteStatus init_status = initializeMicroFeatures();
    if (init_status != kTfLiteOk) {
      return false;
    }

    // Allocate ring buffer
    if (p_buffer == nullptr) {
      p_buffer = new audio_tools::RingBuffer<int16_t>(kMaxAudioSampleSize);
      LOGD("Allocating buffer for %d samples", kMaxAudioSampleSize);
    }

    // Initialize the feature data to default values.
    if (p_feature_data == nullptr) {
      p_feature_data = new int8_t[cfg.featureElementCount()];
      memset(p_feature_data, 0, cfg.featureElementCount());
    }

    // allocate p_audio_samples
    if (p_audio_samples == nullptr) {
      p_audio_samples = new int16_t[kMaxAudioSampleSize];
      memset(p_audio_samples, 0, kMaxAudioSampleSize * sizeof(int16_t));
    }

    return true;
  }

  virtual bool write(int16_t sample) {
    TRACED();
    if (!write1(sample)){
      // determine time
      current_time += cfg.kFeatureSliceStrideMs;
      // determine slice
      total_slice_count++;
      
      int8_t* feature_buffer = addSlice();
      if (total_slice_count >= cfg.kSlicesToProcess) {
        processSlices(feature_buffer);
        // reset total_slice_count
        total_slice_count = 0;
      }
    }
    return true;
  }

 protected:
  TfLiteConfig cfg;
  TfLiteAudioStreamBase *parent=nullptr;
  int8_t* p_feature_data = nullptr;
  int16_t* p_audio_samples = nullptr;
  audio_tools::RingBuffer<int16_t>* p_buffer = nullptr;
  FrontendState g_micro_features_state;
  FrontendConfig config;
  int kMaxAudioSampleSize;
  int kStrideSampleSize;
  int kKeepSampleSize;
  int16_t last_value;
  int8_t channel = 0;
  int32_t current_time = 0;
  int16_t total_slice_count = 0;

  virtual bool setup_recognizer() {
      // setup default p_recognizer if not defined
      if (cfg.recognizeCommands == nullptr) {
        static TfLiteMicroSpeechRecognizeCommands static_recognizer;
        cfg.recognizeCommands = &static_recognizer;
      }
      return cfg.recognizeCommands->begin(cfg);
  }

  /// Processes a single sample 
  virtual bool write1(const int16_t sample) {
    if (cfg.channels == 1) {
      p_buffer->write(sample);
    } else {
      if (channel == 0) {
        last_value = sample;
        channel = 1;
      } else
        // calculate avg of 2 channels and convert it to int8_t
        p_buffer->write(((sample / 2) + (last_value / 2)));
      channel = 0;
    }
    return p_buffer->availableForWrite() > 0;
  }

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
  virtual int8_t* addSlice() {
    TRACED();
    // shift p_feature_data by one slice one one
    memmove(p_feature_data, p_feature_data + cfg.kFeatureSliceSize,
            (cfg.kFeatureSliceCount - 1) * cfg.kFeatureSliceSize);

    // copy data from buffer to p_audio_samples
    int audio_samples_size =
        p_buffer->readArray(p_audio_samples, kMaxAudioSampleSize);

    // check size
    if (audio_samples_size != kMaxAudioSampleSize) {
      LOGE("audio_samples_size=%d != kMaxAudioSampleSize=%d",
           audio_samples_size, kMaxAudioSampleSize);
    }

    // keep some data to be reprocessed - move by kStrideSampleSize
    p_buffer->writeArray(p_audio_samples + kStrideSampleSize, kKeepSampleSize);

    //  the new slice data will always be stored at the end
    int8_t* new_slice_data =
        p_feature_data + ((cfg.kFeatureSliceCount - 1) * cfg.kFeatureSliceSize);
    size_t num_samples_read = 0;
    if (generateMicroFeatures(p_audio_samples, audio_samples_size,
                              new_slice_data, cfg.kFeatureSliceSize,
                              &num_samples_read) != kTfLiteOk) {
      LOGE("Error generateMicroFeatures");
    }
    // printFeatures();
    return p_feature_data;
  }

  // Process multiple slice of audio data 
  virtual bool processSlices(int8_t* feature_buffer) {
    LOGI("->slices: %d", total_slice_count);
    // Copy feature buffer to input tensor
    memcpy(parent->modelInputBuffer(), feature_buffer, cfg.featureElementCount());

    // Run the model on the spectrogram input and make sure it succeeds.
    TfLiteStatus invoke_status = parent->interpreter().Invoke();
    if (invoke_status != kTfLiteOk) {
      LOGE("Invoke failed");
      return false;
    }

    // Obtain a pointer to the output tensor
    TfLiteTensor* output = parent->interpreter().output(0);

    // Determine whether a command was recognized
    const char* found_command = nullptr;
    uint8_t score = 0;
    bool is_new_command = false;

    TfLiteStatus process_status = cfg.recognizeCommands->getCommand(
        output, current_time, &found_command, &score, &is_new_command);
    if (process_status != kTfLiteOk) {
      LOGE("TfLiteMicroSpeechRecognizeCommands::getCommand() failed");
      return false;
    }
    // Do something based on the recognized command. The default
    // implementation just prints to the error console, but you should replace
    // this with your own function for a real application.
    respondToCommand(found_command, score, is_new_command);
    return true;
  }

  /// For debugging: print feature matrix
  void printFeatures() {
    for (int i = 0; i < cfg.kFeatureSliceCount; i++) {
      for (int j = 0; j < cfg.kFeatureSliceSize; j++) {
        Serial.print(p_feature_data[(i * cfg.kFeatureSliceSize) + j]);
        Serial.print(" ");
      }
      Serial.println();
    }
    Serial.println("------------");
  }

  virtual TfLiteStatus initializeMicroFeatures() {
    TRACED();
    config.window.size_ms = cfg.kFeatureSliceDurationMs;
    config.window.step_size_ms = cfg.kFeatureSliceStrideMs;
    config.filterbank.num_channels = cfg.kFeatureSliceSize;
    config.filterbank.lower_band_limit = cfg.filterbank_lower_band_limit;
    config.filterbank.upper_band_limit = cfg.filterbank_upper_band_limit;
    config.noise_reduction.smoothing_bits = cfg.noise_reduction_smoothing_bits;
    config.noise_reduction.even_smoothing = cfg.noise_reduction_even_smoothing;
    config.noise_reduction.odd_smoothing = cfg.noise_reduction_odd_smoothing;
    config.noise_reduction.min_signal_remaining = cfg.noise_reduction_min_signal_remaining;
    config.pcan_gain_control.enable_pcan = cfg.pcan_gain_control_enable_pcan;
    config.pcan_gain_control.strength = cfg.pcan_gain_control_strength;
    config.pcan_gain_control.offset = cfg.pcan_gain_control_offset ;
    config.pcan_gain_control.gain_bits = cfg.pcan_gain_control_gain_bits;
    config.log_scale.enable_log = cfg.log_scale_enable_log;
    config.log_scale.scale_shift = cfg.log_scale_scale_shift;
    if (!FrontendPopulateState(&config, &g_micro_features_state,
                               cfg.sample_rate)) {
      LOGE("frontendPopulateState() failed");
      return kTfLiteError;
    }
    return kTfLiteOk;
  }

  virtual TfLiteStatus generateMicroFeatures(const int16_t* input,
                                             int input_size, int8_t* output,
                                             int output_size,
                                             size_t* num_samples_read) {
    TRACED();
    const int16_t* frontend_input = input;

    // Apply FFT
    FrontendOutput frontend_output = FrontendProcessSamples(
        &g_micro_features_state, frontend_input, input_size, num_samples_read);

    // Check size
    if (output_size != frontend_output.size) {
      LOGE("output_size=%d, frontend_output.size=%d", output_size,
           frontend_output.size);
    }

    // printf("input_size: %d, num_samples_read: %d,output_size: %d,
    // frontend_output.size:%d \n", input_size, *num_samples_read, output_size,
    // frontend_output.size);

    // // check generated features
    // if (input_size != *num_samples_read){
    //   LOGE("audio_samples_size=%d vs num_samples_read=%d", input_size,
    //   *num_samples_read);
    // }

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

  /// Overwrite this method to implement your own handler or provide callback
  virtual void respondToCommand(const char* found_command, uint8_t score,
                                bool is_new_command) {
    if (cfg.respondToCommand != nullptr) {
      cfg.respondToCommand(found_command, score, is_new_command);
    } else {
      TRACED();
      if (is_new_command) {
        char buffer[80];
        snprintf(buffer, 80, "Result: %s, score: %d, is_new: %s", found_command,
                score, is_new_command ? "true" : "false");
        Serial.println(buffer);
      }
    }
  }
};

/**
 * @brief Generate a sine output from a model that was trained on the sine method.
 * (=hello_world)
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteSineReader : public TfLiteReader {
  public: TfLiteSineReader(int16_t range=32767, float increment=0.01 ){
    this->increment = increment;
    this->range = range;
  }

  virtual bool begin(TfLiteAudioStreamBase *parent) override {
    // setup on first call
      p_interpreter = &parent->interpreter();
      input = p_interpreter->input(0);
      output = p_interpreter->output(0);
      channels = parent->config().channels;
      return true;
  }

  virtual int read(int16_t*data, int sampleCount) override {
    TRACED();
    float two_pi = 2 * PI;
    for (int j=0; j<sampleCount; j+=channels){
      // Quantize the input from floating-point to integer
      input->data.int8[0] = TfLiteQuantizer::quantize(actX,input->params.scale, input->params.zero_point);
      
      // Invoke TF Model
      TfLiteStatus invoke_status = p_interpreter->Invoke();

      // Check the result
      if(kTfLiteOk!= invoke_status){
        LOGE("invoke_status not ok");
        return j;
      }
      if(kTfLiteInt8 != output->type){
        LOGE("Output type is not kTfLiteInt8");
        return j;
      }

      // Dequantize the output and convet it to int32 range
      data[j] = TfLiteQuantizer::dequantizeToNewRange(output->data.int8[0], output->params.scale, output->params.zero_point, range);
      // printf("%d\n", data[j]);  // for debugging using the Serial Plotter
      LOGD("%f->%d / %d->%d",actX, input->data.int8[0], output->data.int8[0], data[j]);
      for (int i=1;i<channels;i++){
          data[j+i] = data[j];
          LOGD("generate data for channels");
      }
      // Increment X
      actX += increment;
      if (actX>two_pi){
        actX-=two_pi;
      }
    }
    return sampleCount;
  }

  protected:
    float actX=0;
    float increment=0.1;
    int16_t range=0;
    int channels;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    tflite::MicroInterpreter* p_interpreter = nullptr;
};

/**
 * @brief TfLiteAudioStream which uses Tensorflow Light to analyze the data. If it is used as a generator (where we read audio data) 
 * @ingroup tflite
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class TfLiteAudioStream : public TfLiteAudioStreamBase {
 public:
  TfLiteAudioStream() {}
  ~TfLiteAudioStream() {
    if (p_tensor_arena != nullptr) delete[] p_tensor_arena;
  }


  /// Optionally define your own p_interpreter
  void setInterpreter(tflite::MicroInterpreter* p_interpreter) {
    TRACED();
    this->p_interpreter = p_interpreter;
  }

  // Provides the default configuration
  virtual TfLiteConfig defaultConfig() override {
    TfLiteConfig def;
    return def;
  }

  /// Start the processing
  virtual bool begin(TfLiteConfig config) override {
    TRACED();
    cfg = config;
   
    // alloatme memory
    p_tensor_arena = new uint8_t[cfg.kTensorArenaSize];

    if (cfg.categoryCount()>0){

      // setup the feature provider
      if (!setupWriter()) {
        LOGE("setupWriter");
        return false;
      }
    } else {
      LOGW("categoryCount=%d", cfg.categoryCount());
    }

    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    if (!setModel(cfg.model)) {
      return false;
    }

    if (!setupInterpreter()) {
      return false;
    }

    // Allocate memory from the p_tensor_arena for the model's tensors.
    LOGI("AllocateTensors");
    TfLiteStatus allocate_status = p_interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
      LOGE("AllocateTensors() failed");
      return false;
    }

    // Get information about the memory area to use for the model's input.
    LOGI("Get Input");
    p_tensor = p_interpreter->input(0);
    if (cfg.categoryCount()>0){
      if ((p_tensor->dims->size != 2) || (p_tensor->dims->data[0] != 1) ||
          (p_tensor->dims->data[1] !=
          (cfg.kFeatureSliceCount * cfg.kFeatureSliceSize)) ||
          (p_tensor->type != kTfLiteInt8)) {
        LOGE("Bad input tensor parameters in model");
        return false;
      }
    }

    LOGI("Get Buffer");
    p_tensor_buffer = p_tensor->data.int8;
    if (p_tensor_buffer == nullptr) {
      LOGE("p_tensor_buffer is null");
      return false;
    }

    // setup reader
    if (cfg.reader!=nullptr){
      cfg.reader->begin(this);
    }

    // all good if we made it here
    is_setup = true;
    LOGI("done");
    return true;
  }

  /// Constant streaming
  virtual int availableToWrite() override { return DEFAULT_BUFFER_SIZE; }

  /// process the data in batches of max kMaxAudioSampleSize.
  virtual size_t write(const uint8_t* data, size_t len) override {
    TRACED();
    if (cfg.writer==nullptr){
      LOGE("cfg.output is null");
      return 0;
    }
    int16_t* samples = (int16_t*)data;
    int16_t sample_count = len / 2;
    for (int j = 0; j < sample_count; j++) {
      cfg.writer->write(samples[j]);
    }
    return len;
  }

  /// We can provide only some audio data when cfg.input is defined
  virtual int available() override { return cfg.reader != nullptr ? DEFAULT_BUFFER_SIZE : 0; }

  /// provide audio data with cfg.input 
  virtual size_t readBytes(uint8_t *data, size_t len) override {
    TRACED();
    if (cfg.reader!=nullptr){
      return cfg.reader->read((int16_t*)data, (int) len/sizeof(int16_t)) * sizeof(int16_t);
    }else {
      return 0;
    }
  }

  /// Provides the tf lite interpreter
  tflite::MicroInterpreter& interpreter() override {
    return *p_interpreter;
  }

  /// Provides the TfLiteConfig information
  TfLiteConfig &config() override {
    return cfg;
  }

  /// Provides access to the model input buffer
  int8_t*  modelInputBuffer() override {
    return p_tensor_buffer;
  }

 protected:
  const tflite::Model* p_model = nullptr;
  tflite::MicroInterpreter* p_interpreter = nullptr;
  TfLiteTensor* p_tensor = nullptr;
  bool is_setup = false;
  TfLiteConfig cfg;
  // Create an area of memory to use for input, output, and intermediate
  // arrays. The size of this will depend on the model you're using, and may
  // need to be determined by experimentation.
  uint8_t* p_tensor_arena = nullptr;
  int8_t* p_tensor_buffer = nullptr;

  virtual bool setModel(const unsigned char* model) {
    TRACED();
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

  virtual bool setupWriter() {
    if (cfg.writer == nullptr) {
      static TfLiteMicroSpeachWriter writer;
      cfg.writer = &writer;
    }
    return cfg.writer->begin(this);
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  virtual bool setupInterpreter() {
    if (p_interpreter == nullptr) {
      TRACEI();
      if (cfg.useAllOpsResolver) {
        tflite::AllOpsResolver resolver;
        static tflite::MicroInterpreter static_interpreter{
            p_model, resolver, p_tensor_arena, cfg.kTensorArenaSize};
        p_interpreter = &static_interpreter;
      } else {
        // NOLINTNEXTLINE(runtime-global-variables)
        static tflite::MicroMutableOpResolver<4> micro_op_resolver{};
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
        // Build an p_interpreter to run the model with.
        static tflite::MicroInterpreter static_interpreter{
            p_model, micro_op_resolver, p_tensor_arena, cfg.kTensorArenaSize};
        p_interpreter = &static_interpreter;
      }
    }
    return true;
  }
};

}  // namespace audio_tools