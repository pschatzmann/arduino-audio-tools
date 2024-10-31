

/// set
/// - USB CDC on Boot: enabled
/// - PSRAM: QSPI RAM

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
}

void loop() {

}