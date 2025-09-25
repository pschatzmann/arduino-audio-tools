
#include "Collections/List.h"

namespace audio_tools {

/**
 * @brief Caclulates the moving average of a number of values
 * @ingroup basic
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

template <class N>
class MovingAverage {
 public:
  MovingAverage(size_t size) {
    setSize(size);
  }

  void addMeasurement(N value) {
    if (this->values.size() == this->size) {
      this->values.pop_front();
    }
    this->values.push_back(value);
  }

  float calculate() {
    float sum = 0;
    for (int i = 0; i < this->values.size(); i++) {
      sum += this->values[i];
    }
    return sum / this->values.size();
  }

  /// Defines the number of values
  void setSize(size_t size) {
    this->size = size;
  }

 protected:
  List<N> values;
  size_t size = 0;;
};

}  // namespace audio_tools