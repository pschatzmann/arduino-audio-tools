

#include <cmath>

#pragma once

namespace audio_tools {

/**
 * @brief A simple header only PID Controller
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

class PIDController {
public:
  // dt -  loop interval time
  // max - maximum value of manipulated variable
  // min - minimum value of manipulated variable
  // kp -  proportional gain
  // ki -  Integral gain
  // kd -  derivative gain
  void begin(float dt, float max, float min, float kp, float kd,
                float ki) {
    this->dt = dt;
    this->max = max;
    this->min = min;
    this->kp = kp;
    this->kd = kd;
    this->ki = ki;
  }


  // setpoint = desired process value
  // pv = current process value: return new process value
  float calculate(float target, float measured) {

    // Calculate errori
    float error = target - measured;

    // Proportional term
    float pout = kp * error;

    // Interal term
    integral += error * dt;
    float Iout = ki * integral;

    // Derivative term
    assert(dt!=0.0);
    
    float derivative = (error - preerror) / dt;
    float dout = kd * derivative;

    // Calculate total output
    float output = pout + Iout + dout;

    // Restrict to max/min
    if (output > max)
      output = max;
    else if (output < min)
      output = min;

    // Save error to previous error
    preerror = error;

    return output;
  }

protected:
  float dt = 1.0f;
  float max = 0.0f;
  float min = 0.0f;
  float kp = 0.0f;
  float kd = 0.0f;
  float ki = 0.0f;
  float preerror = 0.0f;
  float integral = 0.0f;

}; // namespace audiotools

} // namespace audiotools
