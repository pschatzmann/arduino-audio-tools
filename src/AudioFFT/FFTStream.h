#pragma once

// Deactivate Multithreading and vector support
#define POCKETFFT_NO_MULTITHREADING
#define POCKETFFT_NO_VECTORS

#include "AudioTools/Streams.h"
#include "AudioTools/MusicalNotes.h"
#include "AudioFFT/FFT.h"
#include <cmath>
#include <cfloat>
#include <cstdlib>

namespace audio_tools {

/**
 * Audio Ouput Stream to perform FFT
 * T defines the audio data (e.g. int16_t) and U the data type which is used for
 * the fft (e.g. float)
 * 
 */
template <class T, class U> class FFTStream : public BufferedStream, public AudioBaseInfoDependent {
public:
  // Default constructor 
  FFTStream(int samplesForFFT = 1024): BufferedStream(DEFAULT_BUFFER_SIZE) {
    max_samples = samplesForFFT;
    array.resize(max_samples);
  }

  void begin(AudioBaseInfo info, WindowFunction wf){
    current_samples = 0;
    this->info = info;
    windowFunction = wf;
  }

  void begin(WindowFunction wf){
    current_samples = 0;
    windowFunction = wf;
  }

  /// Determines the frequency resolution of the FFTArray: Sample Frequency / Number of data points 
  uint32_t frequencyResolution(){
    return info.sample_rate / max_samples;
  }

  /// Provides the mininum frequency in the FFTArray
  uint32_t minFrequency() {
    return frequencyResolution();
  }

  /// Provides the maximum frequency in the FFTArray
  uint32_t maxFrequency() {
    return info.sample_rate;
  }

  /// Determines the frequency at the indicated index of the FFTArray
  uint32_t toFrequency(int idx){
    return minFrequency() + (idx * frequencyResolution());
  }

  /// Determines the amplitude at the indicated index
  U amplitude(FFTArray<U> &stream, int idx){
    return sqrt((stream[idx].real() * stream[idx].real()) + (stream[idx].imag() * stream[idx].imag()));
  }

  /// Determines the index with the max amplitude
  int16_t maxAmplitudeIdx(FFTArray<U> &stream){
    int idx = -1;
    U max = 0;
    for (int j=0;j<current_samples;j++){
      U tmp = amplitude(stream, j);
      if (std::isfinite(tmp) && tmp>max){
        idx = j;
        max = tmp;
      }
    }
    return idx;
  }

  /// Determines the note from the value at max amplitude
  const char* note(FFTArray<U> &array, int &diff){
    int16_t idx = maxAmplitudeIdx(array);
    int16_t frequency = toFrequency(idx);
    return notes.note(frequency, diff); 
  }


  /// Defines the Audio Info
  virtual void setAudioInfo(AudioBaseInfo info) {
    this->info = info;
  };

  AudioBaseInfo audioInfo() {
    return info;
  }


  // defines the callback which processes the fft result
  void setCallback(void (*cb)(FFTStream<T,U> &stream, FFTArray<U> &data)) { 
    this->cb = cb;
  }

protected:
  FFT<U> fft;
  FFTArray<U> array;
  void (*cb)(FFTStream<T,U> &stream, FFTArray<U> &data);
  int max_samples = 0;
  int current_samples = 0;
  AudioBaseInfo info;
  MusicalNotes notes;
  WindowFunction windowFunction;


  /// write data to FFT
  virtual size_t writeExt(const uint8_t *data, size_t len) {
    int size = len / sizeof(T);
    T *ptr = (T*)data;
    for (int j = 0; j < size; j+= info.channels) {
      if (info.channels==1){
        array[current_samples++] = (U)ptr[j];;
      } else {
        U total = 0;
        for (int i=0;i<info.channels;i++){
            total += (U)ptr[j+i];
        }
        array[current_samples++] = total / info.channels;
      }
      // if array is full we calculate the fft
      if (current_samples == max_samples) {
        fft.calculate(array, windowFunction);
        cb(*this, array);
        current_samples = 0;
      }
    }
    return len;
  }

  /// not supported
  virtual size_t readExt(uint8_t *data, size_t len) { return 0; }
};

}