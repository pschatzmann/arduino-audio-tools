// DIAGNOSTIC: bypass AudioBoard/DriverDeviceInfo/AudioDriverWM8994Class
// entirely - instantiate our own WM8994 C++ class directly and drive it
// with plain Wire, nothing else in between. Isolates whether WM8994.h/
// API_I2C.h themselves reproduce the cold-boot failure independent of
// every higher-level abstraction layer.
#include <Wire.h>
#include <AudioDriver.h>
#include <STM32AudioSAI.h>

using namespace audio_driver;

WM8994 wm8994;

void setup() {
  Serial.begin(115200);
  delay(200);
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Info);
  Serial.println("direct WM8994 class test starting...");

  Wire.setSCL(PB8);
  Wire.setSDA(PB9);
  Wire.begin();
  Wire.setClock(400000);

  // Wire.begin() (master mode) sets Init.OwnAddress1 = 0x02 (MASTER_ADDRESS
  // << 1); ST's own I2Cx_Init sets Init.OwnAddress1 = 0. Match ST exactly.
  {
    I2C_HandleTypeDef *hi2c = Wire.getHandle();
    __HAL_I2C_DISABLE(hi2c);
    hi2c->Init.OwnAddress1 = 0;
    hi2c->Init.Timing = 0x40912732;
    HAL_I2C_Init(hi2c);
    __HAL_I2C_ENABLE(hi2c);
  }

  {
    I2C_HandleTypeDef *hi2c = Wire.getHandle();
    Serial.print("hi2c ptr=0x"); Serial.println((uint32_t)(uintptr_t)hi2c, HEX);
    Serial.print("Instance=0x"); Serial.println((uint32_t)(uintptr_t)hi2c->Instance, HEX);
    Serial.print("Timing=0x"); Serial.println(hi2c->Init.Timing, HEX);
    Serial.print("OwnAddress1=0x"); Serial.println(hi2c->Init.OwnAddress1, HEX);
    Serial.print("AddressingMode=0x"); Serial.println(hi2c->Init.AddressingMode, HEX);
    Serial.print("DualAddressMode=0x"); Serial.println(hi2c->Init.DualAddressMode, HEX);
    Serial.print("OwnAddress2=0x"); Serial.println(hi2c->Init.OwnAddress2, HEX);
    Serial.print("GeneralCallMode=0x"); Serial.println(hi2c->Init.GeneralCallMode, HEX);
    Serial.print("NoStretchMode=0x"); Serial.println(hi2c->Init.NoStretchMode, HEX);
    Serial.print("PE bit (CR1&1)=0x"); Serial.println((hi2c->Instance->CR1 & 1), HEX);
  }

  wm8994.setWire(&Wire);
  wm8994.setAddress(0x1A);

  uint32_t rc = wm8994.init(WM8994::OUTPUT_DEVICE_HEADPHONE, 70, WM8994::AUDIO_FREQUENCY_8K);
  Serial.print("wm8994.init rc=");
  Serial.println(rc);

  wm8994.dumpRegisters();
  Serial.println("done...");
}

void loop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last >= 3000) {
    last = now;
    Serial.print("millis=");
    Serial.println(now);
    I2C_HandleTypeDef *hi2c = Wire.getHandle();
    Serial.print("hi2c ptr=0x"); Serial.println((uint32_t)(uintptr_t)hi2c, HEX);
    Serial.print("Instance=0x"); Serial.println((uint32_t)(uintptr_t)hi2c->Instance, HEX);
    Serial.print("Timing=0x"); Serial.println(hi2c->Init.Timing, HEX);
    Serial.print("OwnAddress1=0x"); Serial.println(hi2c->Init.OwnAddress1, HEX);
    Serial.print("AddressingMode=0x"); Serial.println(hi2c->Init.AddressingMode, HEX);
    Serial.print("DualAddressMode=0x"); Serial.println(hi2c->Init.DualAddressMode, HEX);
    Serial.print("OwnAddress2=0x"); Serial.println(hi2c->Init.OwnAddress2, HEX);
    Serial.print("GeneralCallMode=0x"); Serial.println(hi2c->Init.GeneralCallMode, HEX);
    Serial.print("NoStretchMode=0x"); Serial.println(hi2c->Init.NoStretchMode, HEX);
    Serial.print("PE bit (CR1&1)=0x"); Serial.println((hi2c->Instance->CR1 & 1), HEX);
    wm8994.dumpRegisters();
  }
}
