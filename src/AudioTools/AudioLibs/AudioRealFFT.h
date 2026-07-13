#pragma once

#include "AudioFFT.h"
#include "FFT/FFTReal.h"

/** 
 * @defgroup fft-real Real
 * @ingroup fft
 * @brief FFT using Real FFT 
**/

namespace audio_tools {

/**
 * @brief Driver for RealFFT
 * @ingroup fft-real
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriverRealFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            // Recreate the FFT engine whenever the size actually changes --
            // ffft::FFTReal is built for a fixed length, so reusing an
            // already-initialized instance at a different length (e.g. a
            // driver shared/re-begun by multiple consumers at different
            // sizes) would silently run the old, wrong-sized transform on
            // buffers resized for the new length.
            if (p_fft_object != nullptr && this->len != len) {
                delete p_fft_object;
                p_fft_object = nullptr;
            }
            this->len = len;
            v_x.resize(len);
            v_f.resize(len);
            if (p_fft_object==nullptr) p_fft_object = new ffft::FFTReal<float>(len);
            assert(p_fft_object!=nullptr);
            return p_fft_object!=nullptr;
        }
        void end()override{
            if (p_fft_object!=nullptr) {
                delete p_fft_object;
                p_fft_object = nullptr;
            } 
            v_x.resize(0);
            v_f.resize(0);
        }
        void setValue(int idx, float value) override{
            v_x[idx] = value; 
        }

        void fft() override{
            memset(v_f.data(),0,len*sizeof(float));
            p_fft_object->do_fft(v_f.data(), v_x.data());    
        };

        /// Inverse fft - convert fft result back to time domain (samples)
        void rfft() override{
           // ifft
           p_fft_object->do_ifft(v_f.data(), v_x.data());
        }

        bool isReverseFFT() override { return true;}

        float magnitude(int idx) override {
            return sqrt(magnitudeFast(idx));
        }

        // do_fft(f, x) packs the whole spectrum into f (the frequency
        // buffer), leaving x (the time-domain input) untouched -- see
        // FFTReal::do_fft's doc comment: f[0..len/2] = real values of bins
        // 0..len/2 (DC and Nyquist have no imaginary component), and
        // f[len/2+1..len-1] = *negative* imaginary values of bins
        // 1..len/2-1. magnitude/getBin/setBin below all key off that layout
        // in v_f; v_x is only valid to read from after rfft() (inverse).

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override {
            float re = v_f[idx];
            float im = (idx == 0 || idx == len / 2) ? 0.0f : v_f[len / 2 + idx];
            return (re * re) + (im * im);
        }

        bool isValid() override{ return p_fft_object!=nullptr; }

        /// Get the reconstructed time-domain sample. Only meaningful after
        /// rfft() (inverse FFT) writes its result into v_x; right after a
        /// forward fft() this still holds the original input samples, so
        /// use getBin() for frequency-domain results instead.
        float getValue(int idx) override { return v_x[idx];}

        /// Sets bin real/imag as input for the next rfft() (inverse FFT).
        /// Only bins 0..len/2 are meaningful for a real-valued signal; the
        /// negative-imaginary companion for bins 1..len/2-1 is written
        /// automatically in the same packed layout do_fft() produces.
        bool setBin(int pos, float real, float img) override {
            if (pos < 0 || pos > len / 2) return false;
            v_f[pos] = real;
            if (pos > 0 && pos < len / 2) {
                v_f[len / 2 + pos] = -img;
            }
            return true;
        }
        /// Gets bin real/imag from the result of the last forward fft().
        bool getBin(int pos, FFTBin &bin) override {
            if (pos < 0 || pos > len / 2) return false;
            bin.real = v_f[pos];
            bin.img = (pos == 0 || pos == len / 2) ? 0.0f : -v_f[len / 2 + pos];
            return true;
        }

        ffft::FFTReal <float> *p_fft_object=nullptr;
        Vector<float> v_x{0}; // real
        Vector<float> v_f{0}; // complex
        int len;

};

/**
 * @brief AudioFFT using RealFFT
 * @ingroup fft-real
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioRealFFT : public AudioFFTBase {
    public:
        AudioRealFFT():AudioFFTBase(new FFTDriverRealFFT()) {}

        /// Provides the real array returned by the FFT
        float* realArray() {
            return driverEx()->v_x.data();
        }

        /// Provides the complex array returned by the FFT  
        float *imgArray() {
            return driverEx()->v_f.data();
        }

        FFTDriverRealFFT* driverEx() {
            return (FFTDriverRealFFT*)driver();
        }
};

}
