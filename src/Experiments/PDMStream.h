#include "AudioConfig.h"
#include "Stream.h"

namespace audio_tools {

/**
 * @brief Deciates the samples of a sample stream by the indicated factor.
 * Please note that this is a simplified version which assumes that the factor
 * is specified as multiple of the bits_per_sample, so that we can avoid any bit
 * operations. It is also assumed that we have only one channel in the data
 * stream.
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */
template <typename T>
class DecimationStream {
 public:
  DecimationStream(Stream &in) { p_in = &in; }
  DecimationStream(Stream *in) { p_in = in; }

  // defines the decimation factor: as multiple of bits_per sample
  void setFactor(int factor) { dec_factor = factor; }

  size_t readBytes(uint8_t *buffer, size_t byteCount) {
    // make sure that we read full samples
    int samples = byteCount / sizeof(T);
    int result_bytes = samples * sizeof(T);

    // read subsequent samples
    T *data = (T *)&buffer;
    for (int idx = 0; idx < samples; idx++) {
      // decmate subsequent samples
      for (int dec = 0; dec < dec_factor; dec++) {
        if (p_in->readBytes(data + idx, sizeof(T)) != sizeof(T)) {
          LOGE("readBytes failed");
        }
      }
    }

    return result_bytes;
  }

  int available() { return p_in->available() / dec_factor; }

 protected:
  Stream *p_in = nullptr;
  int dec_factor = 1;
};




/**
 * @brief Applies low pass filter to a decimated pdm signal to convert it
 * to pcm
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */

template <typename T>
class PDMStreamT {
 public:
  PDMStreamT(Stream &in) {
    p_in = &in;
    cfg.sample_rate = 44100;
    cfg.channels = 1;
    cfg.bits_per_sample = bitsPerSample();
  }

  // provides the audio information of the PCM stream
  AudioInfo audioInfo() { return cfg; }

  // provides the audio info of the PDM stream (with the much higher sample
  // rate)
  AudioInfo audioInfoPDM() {
    AudioInfo result = audioInfo();
    // sample_rate_pcm = sample_rate_pdm / decimation
    result.sample_rate = cfg.sample_rate * decimation();
    return result;
  }

  // provides the decimation factor that was used in the processing
  int decimation() { return decimation_factor; }

  // defines the decimation factor: must be multiple of bits_per sample
  void setDecimation(int factor) {
    if (factor % bitsPerSample() != 0) {
      LOGW("decimation factor must be multiple of %d", bitsPerSample());
    }
    decimation_factor = factor;
  }

  bool begin(AudioInfo info) {
    if (cfg.channels != 1) {
      LOGE("channels must be 1");
    }
    // set the factor for the decimator
    decimation_stream.setFactor(decimation_factor / sizeof(T) * 8);
    cfg = info;
    return begin();
  }

  bool begin() {
    bool rc = in_filtered.begin(cfg);
    in_filtered.setFilter(0, fir);
    return rc;
  }

  size_t readBytes(uint8_t *buffer, size_t byteCount) {
    // make sure that we read full samples
    int samples = byteCount / sizeof(T);
    int result_bytes = samples * sizeof(T);

    if (in_filtered.readBytes(buffer, result_bytes) != result_bytes) {
      LOGE("readBytes failed");
    }

    return result_bytes;
  }

  int available() { return in_filtered.available(); }

 protected:
  AudioInfo cfg;
  Stream *p_in = nullptr;
  int decimation_factor = sizeof(T) * 8;
  // 44100 hz FIR: cut off: 18040, transition bandwidth: 7380
  float coef[] = {
      -0.000704420658475743, -0.000537879918926308, 0.004114637509913062,
      -0.012685775806621488, 0.027889173789107543,  -0.049285026985058301,
      0.074005079283040689,  -0.097330704866957815, 0.114052040962871595,
      0.880965753382213723,  0.114052040962871595,  -0.097330704866957843,
      0.074005079283040717,  -0.049285026985058301, 0.027889173789107550,
      -0.012685775806621504, 0.004114637509913064,  -0.000537879918926308,
      -0.000704420658475743,
  };
  DecimationStream<T> decimation_stream{p_in};
  FilteredStream<T, float> in_filtered{decimation_stream, 1};
  FIR<float> fir{coef};

  constexpr int8_t bitsPerSample() { return sizeof(T) * 8; }
};

// Define PDMStream
using PDMStream = PDMStreamT<int16_t>;

}  // namespace audio_tools