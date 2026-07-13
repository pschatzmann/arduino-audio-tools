#pragma once

#include "AudioFFT.h"
#include "FixedFFT.h"

/**
 * @defgroup fft-fixed FixedFFT
 * @ingroup fft
 * @brief FFT using fixedpoint-fft (https://github.com/pschatzmann/fixedpoint-fft)
**/

namespace audio_tools {

/**
 * @brief Driver for the fixedpoint-fft library
 * (https://github.com/pschatzmann/fixedpoint-fft). CalcT selects the
 * calculation type used by the underlying engine: int8_t/int16_t/int32_t
 * for fixed point Qn math (smallest/fastest, ideal for plain AVR), or
 * float/double on FPU-equipped targets (ESP32, Cortex-M4F/M7, desktop).
 * Defaults to int16_t (Q15), which gives good precision at any FFT size
 * without needing an FPU.
 * The FFTDriver interface itself is always float, so values are rescaled
 * into/out of CalcT's Qn range on the way in/out via
 * fixedpoint_fft::fftConvertSample.
 * @ingroup fft-fixed
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename CalcT = int16_t>
class FFTDriverFixedFFT : public FFTDriver {
    public:
        bool begin(int len) override {
            this->len = len;
            valid = ffp_object.begin(len);
            if (!valid) {
                LOGE("fixedpoint_fft begin failed: len must be a power of two <= %d",
                     (int)fixedpoint_fft::FixedFFTEngineCore<CalcT>::kMaxN);
            }
            return valid;
        }

        void end() override {
            ffp_object.end();
            valid = false;
        }

        void setValue(int idx, float value) override {
            ffp_object.realData()[idx] = fixedpoint_fft::fftConvertSample<float, CalcT>(value);
            ffp_object.imagData()[idx] = 0;
        }

        void fft() override {
            ffp_object.fft();
        };

        /// Inverse fft - convert fft result back to time domain (samples)
        void rfft() override {
            // safeScale: the spectrum may not have come from fft() (e.g. after setBin())
            ffp_object.ifft(true);
        }

        bool isReverseFFT() override { return true; }

        float magnitude(int idx) override {
            return sqrt(magnitudeFast(idx));
        }

        /// magnitude w/o sqrt
        float magnitudeFast(int idx) override {
            float re = realAsFloat(idx);
            float im = imagAsFloat(idx);
            return re * re + im * im;
        }

        bool isValid() override { return valid; }

        /// get Real value
        float getValue(int idx) override { return realAsFloat(idx); }

        bool setBin(int pos, float real, float img) override {
            if (pos < 0 || pos >= len) return false;
            ffp_object.realData()[pos] = fixedpoint_fft::fftConvertSample<float, CalcT>(real);
            ffp_object.imagData()[pos] = fixedpoint_fft::fftConvertSample<float, CalcT>(img);
            return true;
        }
        bool getBin(int pos, FFTBin &bin) override {
            if (pos < 0 || pos >= len) return false;
            bin.real = realAsFloat(pos);
            bin.img = imagAsFloat(pos);
            return true;
        }

        /// Provides access to the underlying fixedpoint-fft engine
        fixedpoint_fft::FixedFFTEngineCore<CalcT> *engine() { return &ffp_object; }


    protected:
        fixedpoint_fft::FixedFFTEngineCore<CalcT> ffp_object;
        int len = 0;
        bool valid = false;

        float realAsFloat(int idx) {
            return fixedpoint_fft::fftConvertSample<CalcT, float>(ffp_object.real(idx));
        }
        float imagAsFloat(int idx) {
            return fixedpoint_fft::fftConvertSample<CalcT, float>(ffp_object.imag(idx));
        }
};

/**
 * @brief AudioFFT using the fixedpoint-fft library. CalcT selects the
 * calculation type (default int16_t, see FFTDriverFixedFFT); pick
 * int8_t on RAM-constrained plain AVR targets, or float/double on
 * FPU-equipped targets.
 * @ingroup fft-fixed
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
template <typename CalcT = int16_t>
class AudioFixedFFT : public AudioFFTBase {
    public:
        AudioFixedFFT():AudioFFTBase(new FFTDriverFixedFFT<CalcT>()) {}

        /// Provides the real array returned by the FFT
        CalcT* realArray() {
            return driverEx()->engine()->realData();
        }

        /// Provides the imaginary array returned by the FFT
        CalcT* imgArray() {
            return driverEx()->engine()->imagData();
        }

        FFTDriverFixedFFT<CalcT>* driverEx() {
            return (FFTDriverFixedFFT<CalcT>*)driver();
        }
};

}
