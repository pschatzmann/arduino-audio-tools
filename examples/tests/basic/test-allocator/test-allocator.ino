#include "AudioTools.h"

struct X {
  X() { value = 'a'; }
  char value;
};

Vector<X> vector{10, DefaultAllocator};

void setup() {
  Serial.begin(115200);

  int count = 0;
  //vector.resize(10);
  for (auto &ref : vector) {
    Serial.print(ref.value);
    assert(ref.value == 'a');
    count++;
  }
  assert(count == 10);
  Serial.println();
  Serial.println("test ok");
}

void loop() {}