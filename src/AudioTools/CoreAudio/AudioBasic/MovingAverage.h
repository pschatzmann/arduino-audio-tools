
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
    if (this->values.empty()) return 0;
    float sum = 0;
    for (auto it = this->values.begin(); it != this->values.end(); ++it) {
      sum += *it;
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