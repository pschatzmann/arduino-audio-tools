
#include "AudioTools.h"

void print(const char *title, Vector<int> &vector) {
  Serial.println(title);
  for (int j = 0; j < vector.size(); j++) {
    Serial.print(vector[j]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println();
}

void testPushBack() {
  Vector<int> vector;
  for (int j = 0; j < 10; j++) {
    vector.push_back(j);
  }
  print("testPushBack", vector);
  for (int j = 0; j < 10; j++) {
    assert(vector[j] == j);
  }
}

void testPushFront() {
  Vector<int> vector;
  for (int j = 0; j < 10; j++) {
    vector.push_front(j);
  }
  print("testPushFront", vector);

  for (int j = 0; j < 10; j++) {
    assert(vector[9 - j] == j);
  }
}

void testPopFront() {
  Vector<int> vector;
  for (int j = 0; j < 10; j++) {
    vector.push_back(j);
  }
  vector.pop_front();
  print("testPopFront", vector);

  assert(vector.size() == 9);
  for (int j = 0; j < 9; j++) {
    assert(vector[j] == j + 1);
  }
}

void testPopBack() {
  Vector<int> vector;
  for (int j = 0; j < 10; j++) {
    vector.push_back(j);
  }
  vector.pop_back();
  print("testPopBack", vector);

  assert(vector.size() == 9);
  for (int j = 0; j < 9; j++) {
    assert(vector[j] == j);
  }
}

void testErase() {
  Vector<int> vector;
  for (int j = 0; j < 10; j++) {
    vector.push_back(j);
  }
  vector.erase(0);
  print("testErase", vector);

  assert(vector.size() == 9);
  for (int j = 0; j < 9; j++) {
    assert(vector[j] == j + 1);
  }
}

void setup() {
  Serial.begin(115200);
  testPushBack();
  testPushFront();
  testPopFront();
  testPopBack();
  testErase();
  Serial.print("All tests passed");
}

void loop() {}