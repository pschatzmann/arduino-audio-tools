#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace audio_tools {

/**
 * @brief Class for collecting and analyzing S/PDIF pulse histograms.
 *
 * This class provides methods to collect pulse width histograms and analyze
 * them to determine timing characteristics for S/PDIF decoding.
 * @author Nathan Ladwig (netham45)
 * @author Phil Schatzmann (pschatzmann)
 */
class SPDIFHistogram {
 public:
  /** Number of histogram bins for pulse width. */
  static constexpr int HISTOGRAM_BINS = 256;
  /**
   * @brief Validation results for pulse timing analysis.
   */
  struct TimingValidation {
    bool groups_identified = false;
    bool ratios_valid = false;
    bool distribution_valid = false;
    float ratio_error = 0.0f;
    float short_pulse_pct = 0.0f;
    float medium_pulse_pct = 0.0f;
    float long_pulse_pct = 0.0f;
    float distribution_error = 0.0f;
  };

  /**
   * @brief Stores timing and histogram data for analysis.
   */
  struct Timing {
    uint32_t histogram[HISTOGRAM_BINS] = {0};
    uint32_t total_samples = 0;
    uint32_t base_unit_ticks = 0;
    uint32_t short_pulse_ticks = 0;
    uint32_t medium_pulse_ticks = 0;
    uint32_t long_pulse_ticks = 0;
    uint32_t short_medium_threshold = 0;
    uint32_t medium_long_threshold = 0;
    bool timing_discovered = false;
    uint32_t last_analysis_time = 0;
    TimingValidation last_validation;
  };

  /**
   * @brief Timing and histogram data for this instance.
   */

  inline Timing getTiming() { return timing; }

  /**
   * @brief Collects pulse width histogram from symbol durations.
   * @param symbols Array of pulse durations.
   * @param num_symbols Number of symbols in the array.
   */
  inline void collectPulseHistogram(const uint32_t* symbols,
                                    size_t num_symbols) {
    for (size_t i = 0; i < num_symbols; i++) {
      uint32_t dur = symbols[i];
      if (dur > 0 && dur < HISTOGRAM_BINS) {
        timing.histogram[dur]++;
        timing.total_samples++;
      }
    }
  }

  /**
   * @brief Analyzes the collected histogram to identify pulse timing groups and
   * thresholds.
   */
  inline void analyzePulseTiming() {
    uint32_t smoothed[HISTOGRAM_BINS] = {0};
    smoothHistogram(timing.histogram, smoothed, HISTOGRAM_BINS);
    Peak peaks[10] = {};
    int num_peaks = 0;
    uint32_t max_count = 0;
    for (int i = 0; i < HISTOGRAM_BINS; i++) {
      if (smoothed[i] > max_count) max_count = smoothed[i];
    }
    uint32_t min_peak_height = (max_count / 50 > timing.total_samples / 200)
                                   ? max_count / 50
                                   : timing.total_samples / 200;
    for (int i = 2; i < HISTOGRAM_BINS - 2 && num_peaks < 10; i++) {
      if (smoothed[i] > min_peak_height && smoothed[i] >= smoothed[i - 1] &&
          smoothed[i] >= smoothed[i - 2] && smoothed[i] >= smoothed[i + 1] &&
          smoothed[i] >= smoothed[i + 2]) {
        bool is_distinct = true;
        for (int j = 0; j < num_peaks; j++) {
          if (std::abs((int)i - (int)peaks[j].bin) < 8) {
            if (smoothed[i] > peaks[j].count) {
              peaks[j].bin = i;
              peaks[j].count = smoothed[i];
              peaks[j].center = findPeakCenter(smoothed, i, 3);
            }
            is_distinct = false;
            break;
          }
        }
        if (is_distinct) {
          peaks[num_peaks].bin = i;
          peaks[num_peaks].count = smoothed[i];
          peaks[num_peaks].center = findPeakCenter(smoothed, i, 3);
          num_peaks++;
        }
      }
    }
    if (num_peaks < 3) return;
    for (int i = 0; i < num_peaks - 1; i++) {
      for (int j = i + 1; j < num_peaks; j++) {
        if (peaks[i].center > peaks[j].center) {
          Peak temp = peaks[i];
          peaks[i] = peaks[j];
          peaks[j] = temp;
        }
      }
    }
    int best_set[3] = {-1, -1, -1};
    float best_error = 1000.0f;
    for (int i = 0; i < num_peaks - 2; i++) {
      for (int j = i + 1; j < num_peaks - 1; j++) {
        for (int k = j + 1; k < num_peaks; k++) {
          float ratio1 = peaks[j].center / peaks[i].center;
          float ratio2 = peaks[k].center / peaks[i].center;
          float error = std::fabs(ratio1 - 2.0f) + std::fabs(ratio2 - 3.0f);
          if (error < best_error) {
            best_error = error;
            best_set[0] = i;
            best_set[1] = j;
            best_set[2] = k;
          }
        }
      }
    }
    if (best_set[0] >= 0 && best_error < PULSE_RATIO_TOLERANCE * 2) {
      Peak selected_peaks[3];
      for (int i = 0; i < 3; i++) selected_peaks[i] = peaks[best_set[i]];
      float ratio1 = selected_peaks[1].center / selected_peaks[0].center;
      float ratio2 = selected_peaks[2].center / selected_peaks[0].center;
      TimingValidation validation = validatePulseDistribution(
          selected_peaks, 3, ratio1, ratio2, best_error);
      timing.last_validation = validation;
      if (validation.groups_identified && validation.ratios_valid &&
          validation.distribution_valid) {
        timing.base_unit_ticks = (uint32_t)(selected_peaks[0].center * 2);
        timing.short_pulse_ticks = (uint32_t)selected_peaks[0].center;
        timing.medium_pulse_ticks = (uint32_t)selected_peaks[1].center;
        timing.long_pulse_ticks = (uint32_t)selected_peaks[2].center;
        timing.timing_discovered = true;
        calculateAdaptiveThresholds();
      }
    }
  }

 protected:
  /** Maximum expected pulse width in nanoseconds. */
  static constexpr int MAX_PULSE_WIDTH_NS = 2000;
  /** Minimum samples before attempting analysis. */
  static constexpr int MIN_SAMPLES_FOR_ANALYSIS = 10000;
  /** Tolerance for pulse ratio matching. */
  static constexpr float PULSE_RATIO_TOLERANCE = 0.15f;
  /** Expected short pulse percentage. */
  static constexpr float EXPECTED_SHORT_PULSE_PCT = 60.0f;
  /** Expected medium pulse percentage. */
  static constexpr float EXPECTED_MEDIUM_PULSE_PCT = 35.0f;
  /** Expected long pulse percentage. */
  static constexpr float EXPECTED_LONG_PULSE_PCT = 5.0f;
  /** Tolerance for distribution validation. */
  static constexpr float DISTRIBUTION_TOLERANCE = 100.0f;

  /**
   * @brief Represents a peak in the histogram.
   */
  struct Peak {
    uint32_t bin = 0;
    uint32_t count = 0;
    float center = 0.0f;
    uint32_t width = 0;
  };

  Timing timing;

  /**
   * @brief Smooths histogram data using a 3-point moving average.
   * @param input Input histogram array.
   * @param output Output smoothed histogram array.
   * @param size Size of the histogram arrays.
   */
  void smoothHistogram(const uint32_t* input, uint32_t* output, int size) {
    output[0] = input[0];
    output[size - 1] = input[size - 1];
    for (int i = 1; i < size - 1; i++) {
      output[i] = (input[i - 1] + input[i] + input[i + 1]) / 3;
    }
  }
  /**
   * @brief Finds the center of mass for a peak in the histogram.
   * @param histogram Histogram array.
   * @param peak_bin Index of the peak bin.
   * @param window Number of bins to include around the peak.
   * @return Center of mass for the peak.
   */
  float findPeakCenter(const uint32_t* histogram, int peak_bin, int window) {
    float weighted_sum = 0;
    float weight_total = 0;
    int start = (peak_bin - window >= 0) ? peak_bin - window : 0;
    int end = (peak_bin + window < HISTOGRAM_BINS) ? peak_bin + window
                                                   : HISTOGRAM_BINS - 1;
    for (int i = start; i <= end; i++) {
      weighted_sum += i * histogram[i];
      weight_total += histogram[i];
    }
    return (weight_total > 0) ? weighted_sum / weight_total : peak_bin;
  }
  /**
   * @brief Calculates adaptive thresholds between pulse groups.
   */
  void calculateAdaptiveThresholds() {
    if (!timing.timing_discovered) return;
    timing.short_medium_threshold =
        (timing.short_pulse_ticks + timing.medium_pulse_ticks) / 2;
    timing.medium_long_threshold =
        (timing.medium_pulse_ticks + timing.long_pulse_ticks) / 2;
  }
  /**
   * @brief Validates pulse distribution against S/PDIF specification.
   * @param peaks Array of detected peaks.
   * @param num_peaks Number of peaks in the array.
   * @param ratio1 Ratio of medium to short pulse.
   * @param ratio2 Ratio of long to short pulse.
   * @param best_error Best error value for ratio matching.
   * @return Validation results.
   */
  TimingValidation validatePulseDistribution(const Peak* peaks, int num_peaks,
                                             float ratio1, float ratio2,
                                             float best_error) {
    TimingValidation result;
    result.groups_identified = (num_peaks >= 3);
    if (!result.groups_identified) return result;
    result.ratio_error = best_error;
    result.ratios_valid = (std::fabs(ratio1 - 2.0f) < PULSE_RATIO_TOLERANCE &&
                           std::fabs(ratio2 - 3.0f) < PULSE_RATIO_TOLERANCE);
    uint32_t total = peaks[0].count + peaks[1].count + peaks[2].count;
    if (total > 0) {
      result.short_pulse_pct = 100.0f * peaks[0].count / total;
      result.medium_pulse_pct = 100.0f * peaks[1].count / total;
      result.long_pulse_pct = 100.0f * peaks[2].count / total;
      float short_error =
          std::fabs(result.short_pulse_pct - EXPECTED_SHORT_PULSE_PCT);
      float medium_error =
          std::fabs(result.medium_pulse_pct - EXPECTED_MEDIUM_PULSE_PCT);
      float long_error =
          std::fabs(result.long_pulse_pct - EXPECTED_LONG_PULSE_PCT);
      result.distribution_error = short_error + medium_error + long_error;
      result.distribution_valid = (short_error <= DISTRIBUTION_TOLERANCE &&
                                   medium_error <= DISTRIBUTION_TOLERANCE &&
                                   long_error <= DISTRIBUTION_TOLERANCE);
    }
    return result;
  }
};

}  // namespace audio_tools
