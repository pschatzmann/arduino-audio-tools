//
// Bare I2C bus diagnostic for the STM32F723E-Discovery's WM8994 codec bus
// (I2C1 on PB8=SCL/PB9=SDA per the ST BSP). No AudioTools/audio-driver
// involved at all - isolates whether the raw I2C bus/pins work before
// blaming the codec driver.
//
// Expected result if healthy: address 0x1A (WM8994, 7-bit form of the BSP's
// 8-bit 0x34) answers, ideally alongside nothing else on this dedicated bus.
// If NOTHING answers, and the audio-out.ino sketch logged repeated
// "endTransmission: 4" (I2C_TIMEOUT/BUSY/ERROR, not a clean NACK=2), that
// points to an electrical-level issue: missing/weak pull-ups on PB8/PB9,
// a stuck bus, or a hardware fault - not a pin/address/driver bug.
//
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(500);

  Serial.println("I2C1 scanner: SCL=PB8, SDA=PB9");
  Wire.setSCL(PB8);
  Wire.setSDA(PB9);
  Wire.begin();
  Wire.setClock(100000);
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t rc = Wire.endTransmission();
    if (rc == 0) {
      Serial.print("Found device at 0x");
      Serial.println(addr, HEX);
      found++;
    } else if (rc != 2) {
      // rc==2 is a clean NACK (no device at this address) - anything else
      // (1,3,4) is worth calling out explicitly since it signals a bus-level
      // problem rather than just "not present".
      Serial.print("Unexpected error at 0x");
      Serial.print(addr, HEX);
      Serial.print(": rc=");
      Serial.println(rc);
    }
  }
  if (found == 0) {
    Serial.println("No devices found.");
  }
  Serial.println("---scan done, repeating in 3s---");
  delay(3000);
}
