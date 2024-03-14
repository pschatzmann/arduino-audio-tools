#include "Arduino.h"

/**
 * @brief Pin Tests for AudioKit
 * 
 */

void setup() {
    Serial.begin(115200);
    for (int j=10;j<=36;j++){
        pinMode(j, INPUT_PULLUP);
    }
}

void loop() {
    for (int j=10;j<=36;j++){
        int value = digitalRead(j);
        Serial.print(value ? "-":"+");
    }
    Serial.println();
    delay(1000);
}