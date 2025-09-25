#pragma once

namespace audio_tools {

/**
 * @brief Simple 1D Kalman Filter for smoothing measurements.
 *
 * This class implements a basic one-dimensional Kalman filter for smoothing
 * noisy measurements. It is useful for sensor data fusion and signal processing
 * applications where noise reduction is required.
 *
 * The Kalman filter uses two main parameters:
 *   - @b process_noise (Q): Represents the expected process variance. A typical
 * default is 0.01.
 *   - @b measurement_noise (R): Represents the expected measurement variance. A
 * typical default is 1.0.
 *
 * Lower values for Q make the filter trust the model more (less responsive to
 * changes), while higher values for R make the filter trust the measurements
 * less (more smoothing).
 *
 * @note Reasonable defaults for many sensor applications are Q = 0.01 and R
 * = 1.0, but these should be tuned for your specific use case.
 * @ingroup filters
 * @author Phil Schatzmann
 */
class KalmanFilter {
 public:
  /**
   * @brief Construct a new Kalman Filter object
   *
   * @param process_noise The process noise covariance (Q). Default is 0.01.
   * @param measurement_noise The measurement noise covariance (R). Default
   * is 1.0.
   */
  KalmanFilter(float process_noise = 0.01f, float measurement_noise = 1.0f) {
    begin(process_noise, measurement_noise);
  }

  /**
   * @brief reset the filter with new parameters
   *
   * @param process_noise The process noise covariance (Q). Default is 0.01.
   * @param measurement_noise The measurement noise covariance (R). Default
   * is 1.0.
   */

  /**
   * @brief Initialize or reset the filter with new parameters.
   *
   * @param process_noise The process noise covariance (Q). Default is 0.01.
   * @param measurement_noise The measurement noise covariance (R). Default
   * is 1.0.
   * @return true Always returns true.
   */
  bool begin(float process_noise, float measurement_noise) {
    Q = process_noise;
    R = measurement_noise;
    P = 1.0;
    X = 0.0;
    return true;
  }

  /**
   * @brief Reset the filter state to zero, keeping the current noise
   * parameters.
   *
   * @return true Always returns true.
   */
  bool begin() {
    X = 0;
    return true;
  }

  /**
   * @brief End or clear the filter (sets the estimate to zero).
   */
  void end() { X = 0; }

  /**
   * @brief Updates the filter with a new measurement and returns the filtered
   * value.
   *
   * @param measurement The new measurement to be filtered
   * @return float The updated (filtered) estimate
   */
  void addMeasurement(float measurement) {
    // Prediction update
    P = P + Q;

    // Measurement update
    K = P / (P + R);
    X = X + K * (measurement - X);
    P = (1 - K) * P;
  }

  /**
   * @brief Returns the current estimated value.
   *
   * @return float The current estimate
   */
  float calculate() { return X; }

 protected:
  /**
   * @brief Process noise covariance (Q)
   */
  float Q;
  /**
   * @brief Measurement noise covariance (R)
   */
  float R;
  /**
   * @brief Estimation error covariance (P)
   */
  float P;
  /**
   * @brief Estimated state (X)
   */
  float X;
  /**
   * @brief Kalman gain (K)
   */
  float K;
};

}  // namespace audio_tools