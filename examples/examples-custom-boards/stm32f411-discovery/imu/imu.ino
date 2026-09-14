//
// IMU Example for STM32F411 Discovery Board
// Reads the on-board ST MEMS sensors:
//  - Gyroscope: L3GD20 or I3G4250D (SPI1: SCK=PA5, MISO=PA6, MOSI=PA7, CS=PE3)
//  - Accelerometer/Magnetometer: LSM303AGR (I2C1: SCL=PB6, SDA=PB9)
//
// Dependencies:
//  - L3GD20_I3G4250D (header-only):
//    no existing Arduino library speaks both SPI and recognizes the
//    I3G4250D's WHO_AM_I (0xD3), so this small driver was written for this
//    sketch: https://github.com/pschatzmann/L3GD20_I3G4250D
//    Reference chip datasheets:
//      https://www.st.com/resource/en/datasheet/l3gd20.pdf
//      https://www.st.com/resource/en/datasheet/i3g4250d.pdf
//  - STM32duino LSM303AGR (targets the LSM303AGR specifically, not the
//    older LSM303DLHC):
//      https://github.com/stm32duino/LSM303AGR
//      Install: Arduino IDE Library Manager, or
//      `arduino-cli lib install "STM32duino LSM303AGR"`
//

#include <SPI.h>
#include <Wire.h>
#include <L3GD20_I3G4250D.h> // https://github.com/pschatzmann/L3GD20_I3G4250D
#include <LSM303AGR_ACC_Sensor.h> // https://github.com/stm32duino/LSM303AGR
#include <LSM303AGR_MAG_Sensor.h> // https://github.com/stm32duino/LSM303AGR

// ---- Pin mapping (STM32F411E-Discovery schematic) ----
#define GYRO_CS PE3
#define GYRO_SCK PA5
#define GYRO_MISO PA6
#define GYRO_MOSI PA7

#define I2C_SCL PB6
#define I2C_SDA PB9

SPIClass gyroSPI(GYRO_MOSI, GYRO_MISO, GYRO_SCK);
TwoWire imuWire(I2C_SDA, I2C_SCL);

L3GD20_I3G4250D Gyro(gyroSPI, GYRO_CS);
LSM303AGR_ACC_Sensor Acc(&imuWire);
LSM303AGR_MAG_Sensor Mag(&imuWire);

// Arduino Setup
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);

  if (Gyro.begin()) {
    Serial.print("Gyroscope: ");
    Serial.println(Gyro.chipName());
  } else {
    Serial.println("Gyroscope: not detected!");
  }

  imuWire.begin();
  Acc.begin();
  Acc.Enable();
  Mag.begin();
  Mag.Enable();
}

// Arduino Loop
void loop() {
  float gx, gy, gz;
  Gyro.readDPS(gx, gy, gz);

  int32_t acc[3];
  Acc.GetAxes(acc);  // mg

  int32_t mag[3];
  Mag.GetAxes(mag);  // mGauss

  Serial.print("gyro(dps): ");
  Serial.print(gx);
  Serial.print(", ");
  Serial.print(gy);
  Serial.print(", ");
  Serial.print(gz);

  Serial.print("  acc(mg): ");
  Serial.print(acc[0]);
  Serial.print(", ");
  Serial.print(acc[1]);
  Serial.print(", ");
  Serial.print(acc[2]);

  Serial.print("  mag(mGa): ");
  Serial.print(mag[0]);
  Serial.print(", ");
  Serial.print(mag[1]);
  Serial.print(", ");
  Serial.println(mag[2]);

  delay(500);
}
