#pragma once
#include "AudioTools/FFT.h"
#include "AudioTools/Streams.h"

namespace audio_tools {

/**
 * Audio Ouput Stream to perform FFT
 * T defines the audio data (e.g. int16_t) and U the data type which is used for
 * the fft (e.g. float)
 */
template <class T, class U> class FFTStream : public BufferedStream {
public:
  // Default constructor 
  FFTStream(int channels=1, int samplesForFFT = 1024): BufferedStream(DEFAULT_BUFFER_SIZE) {
      begin(channels, samplesForFFT);
  }

  void begin(int channels=1, int samplesForFFT=1024){
    max_samples = samplesForFFT;
    array.resize(max_samples);
    current_samples = 0;
    this->channels = channels;
  }

  // defines the callback which processes the fft result
  void setCallback(void (*cb)(FFTArray<U> &data)) { this->cb = cb; }

protected:
  FFT<U> fft;
  FFTArray<U> array;
  void (*cb)(FFTArray<U> &data);
  int max_samples = 0;
  int current_samples = 0;
  int channels = 1;

  /// write data to FFT
  virtual size_t writeExt(const uint8_t *data, size_t len) {
    int size = len / sizeof(T);
    T *ptr = (T*)data;
    for (int j = 0; j < size; j+=channels) {
      if (channels==1){
        array[current_samples++] = (U)ptr[j];;
      } else {
        U total = 0;
        for (int i=0;i<channels;i++){
            total += (U)ptr[j+i];
        }
        array[current_samples++] = total / channels;
      }
      // if array is full we calculate the fft
      if (current_samples == max_samples) {
        fft.calculate(array);
        cb(array);
        current_samples = 0;
      }
    }
  }

  /// not supported
  virtual size_t readExt(uint8_t *data, size_t len) { return 0; }
};

}