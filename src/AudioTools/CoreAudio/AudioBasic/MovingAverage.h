
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
  MovingAverage(int size) {
    this->size = size;
    this->values = List<float>();
    this->values.reserve(size);
  }

  void add(float value) {
    if (this->values.size() == this->size) {
      this->values.pop_front();
    }
    this->values.push_back(value);
  }

  float average() {
    float sum = 0;
    for (int i = 0; i < this->values.size(); i++) {
      sum += this->values[i];
    }
    return sum / this->values.size();
  }

 protected:
  List<N> values;
  int size;
};

}  // namespace audio_tools