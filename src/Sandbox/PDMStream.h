#include "AudioConfig.h"
#include "Stream.h"

#define DELAY(j) {for(volatile int i=0;i<j;i++);}

namespace audio_tools {

/**
 * @brief Deciates an sample stream by the indicated factor:
 * Decimation counts the number of set bits.
 * Please note that the factor is specified as multiple of the bits_per_sample.
 * It is also assumed that we have only one channel in the data
 * stream.
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */
template <typename T>
class DecimationStreamExt : public AudioStream {
 public:
  DecimationStreamExt() = default;
  DecimationStreamExt(Stream &in) { setStream(in); }

  void setStream(Stream &in) { p_in = &in; }

  virtual bool begin(AudioInfo cfg) {
    setAudioInfo(cfg);
    return true;
  }

  // defines the decimation factor: as multiple of bits_per sample
  void setDecimationFactor(int factor) { dec_factor = factor; }

  size_t readBytes(uint8_t *buffer, size_t byteCount) {
    LOGD("readBytes:%d", byteCount);
    assert(buffer != nullptr);
    // make sure that we read full samples
    int samples = byteCount / sizeof(T);
    int result_bytes = samples * sizeof(T);

    // calculated scaling factor and offset for unsigned to signed conversion
    int max_value = sizeof(T) * 8 * dec_factor;  // e.g. 16 * 32 = 512
    int scaled_max = pow(2, sizeof(T) * 8);      // e.g. 65536
    int factor = scaled_max / max_value;         // e.g. 128
    int unsigned_to_signed = scaled_max / 2;     // e.g. 32768

    // provide subsequent samples
    T *data = (T *)buffer;
    T tmp_in;
    for (int idx = 0; idx < samples; idx++) {
      // decimate subsequent samples
      unsigned decimated = 0;
      for (int dec = 0; dec < dec_factor; dec++) {
        int sample_bytes = sizeof(T);
        LOGD("readBytes: %d", sample_bytes);
        if (p_in->readBytes((uint8_t *)&tmp_in, sample_bytes) != sample_bytes) {
          LOGE("readBytes failed");
        }
        decimated += countSetBits(tmp_in);
      }
      // store scaled decimated as singned value
      //data[idx] = (decimated * factor) - unsigned_to_signed;
      data[idx] = decimated;
    }

    return result_bytes;
  }

  int available() {
    TRACED();
    return 1024;
  }

  AudioInfo audioInfoPDM() {
    AudioInfo result = audioInfo();
    // sample_rate_pcm = sample_rate_pdm / decimation
    result.sample_rate = info.sample_rate * dec_factor;
    return result;
  }

 protected:
  Stream *p_in = nullptr;
  int dec_factor = 32;  // usually set between 25 and 64×

  unsigned countSetBits(unsigned n) {
    TRACED();
    T count = 0;
    while (n) {
      count += n & 1;
      n >>= 1;
    }
    return count;
  }
};


/*
We read the raw PDM data with the help of the Arduino digitalRead(). The SEL pin
needs to be connected to GND, so that the data is valid at clock pin low.

COMMON CLOCK AND DECIMATION RATIO SETTINGS
PDM Clock Frequency(fPDM)Decimation Ratio Baseband Sampling Rate (fs) Audio Signal Bandwidth (fs/2)Applications
4.8 MHz 25× 192 kHz 96 kHz Ultrasound
4.8 MHz 50× 96 kHz 48 kHz
3.072 MHz 64× 48 kHz 24 kHz Full-bandwidth audio
2.4 MHz 50× 48 kHz 24 kHz
1.536 MHz 48× 32 kHz 16 kHz Wide-bandwidth audio
1.024 MHz 64× 16 kHz 8 kHz High-quality voice
768 kHz 48× 16 kHz 8 kHz
512 kHz 32× 16 kHz 8 kHz
512 kHz 64× 8 kHz 4 kHz Standard voice
*/

template <typename T>
class BitBangDecimationStream : public DecimationStreamExt<T> {

public:
  bool begin(AudioInfo cfg) {
    DecimationStreamExt<T>::setAudioInfo(cfg);
    pinMode(pin_clock, OUTPUT);
    pinMode(pin_data, INPUT);
    return true;
  }

  size_t readBytes(uint8_t *buffer, size_t byteCount) {
    LOGD("readBytes:%d", byteCount);
    assert(buffer != nullptr);
    // make sure that we read full samples
    int samples = byteCount / sizeof(T);
    int result_bytes = samples * sizeof(T);

    // calculated scaling factor and offset for unsigned to signed conversion
    int max_value = sizeof(T) * 8 * DecimationStreamExt<T>::dec_factor;  // e.g. 16 * 32 = 512
    int scaled_max = pow(2, sizeof(T) * 8);      // e.g. 65536
    int factor = scaled_max / max_value;         // e.g. 128
    int unsigned_to_signed = scaled_max / 2;     // e.g. 32768

    // provide subsequent samples
    T *data = (T *)buffer;
    T tmp_in;
    for (int idx = 0; idx < samples; idx++) {
      // decimate subsequent samples
      unsigned decimated = 0;
      for (int dec = 0; dec < DecimationStreamExt<T>::dec_factor * sizeof(T)*8; dec++) {
        int sample_bytes = sizeof(T);
        digitalWrite(pin_clock, HIGH);
        DELAY(1);
        digitalWrite(pin_clock, LOW);
        bool bit = digitalRead(pin_data);
        DELAY(1);
        // sum up active bits
        decimated += bit;
      }
      // store scaled decimated as singned value
      //data[idx] = (decimated * factor) - unsigned_to_signed;
      data[idx] = decimated;
    }

    return result_bytes;
  }

protected:
  int pin_clock = 14;
  int pin_data = 32;
};

/**
 * @brief Applies low pass filter to a decimated pdm signal to convert it
 * to pcm
 * @author Phil Schatzmann
 * @ingroup io
 * @copyright GPLv3
 */

template <typename T>
class PDMMonoStreamT : public AudioStream {
 public:
  PDMMonoStreamT(Stream &in) {
    decimation_stream.setStream(in);
    info.sample_rate = 44100;
    info.channels = 1;
    info.bits_per_sample = bitsPerSample();
  }

  // provides the audio info of the PDM stream (with the much higher sample
  // rate)
  AudioInfo audioInfoPDM() {
    AudioInfo result = audioInfo();
    // sample_rate_pcm = sample_rate_pdm / decimation
    result.sample_rate = info.sample_rate * decimation();
    return result;
  }

  // provides the decimation factor that was used in the processing
  int decimation() { return decimation_factor; }

  // defines the decimation factor: must be multiple of bits_per sample
  void setDecimationFactor(int factor) {
    decimation_factor = factor;
  }

  bool begin(AudioInfo info) {
    setAudioInfo(info);
    if (info.channels != 1) {
      LOGE("channels must be 1");
    }
    // set the factor for the decimator
    decimation_stream.setDecimationFactor(decimation_factor);
    return begin();
  }

  bool begin() {
    decimation_stream.begin(info);
    bool rc = in_filtered.begin(info);
    in_filtered.setFilter(0, fir);
    return rc;
  }

  size_t readBytes(uint8_t *buffer, size_t byteCount) {
    LOGD("readBytes:%d", byteCount);
    assert(buffer != nullptr);
    // make sure that we read full samples
    int samples = byteCount / sizeof(T);
    int result_bytes = samples * sizeof(T);
    assert(result_bytes <= byteCount);

    if (in_filtered.readBytes(buffer, result_bytes) != result_bytes) {
      LOGE("readBytes failed");
    }

    return result_bytes;
  }

  int available() {
    TRACED();
    return in_filtered.available();
  }

  template <size_t B>
  void setFilterValues(const T (&b)[B]) {
    fir.setValues(b);
  }

 protected:
  int decimation_factor = sizeof(T) * 2;
  // 44100 hz FIR: cut off: 18040, transition bandwidth: 7380
  float coef[19] = {
      -0.000704420658475743, -0.000537879918926308, 0.004114637509913062,
      -0.012685775806621488, 0.027889173789107543,  -0.049285026985058301,
      0.074005079283040689,  -0.097330704866957815, 0.114052040962871595,
      0.880965753382213723,  0.114052040962871595,  -0.097330704866957843,
      0.074005079283040717,  -0.049285026985058301, 0.027889173789107550,
      -0.012685775806621504, 0.004114637509913064,  -0.000537879918926308,
      -0.000704420658475743};
  DecimationStreamExt<T> decimation_stream;
  FilteredStream<T, float> in_filtered{decimation_stream, 1};
  FIR<float> fir{coef};

  constexpr int8_t bitsPerSample() { return sizeof(T) * 8; }
};

// Define PDMStream
using PDMMonoStream = PDMMonoStreamT<int16_t>;

}  // namespace audio_tools