Apart from the sketches that you can find in this directory, I recommend that you use the __USB audio functionality__ that is available for the STM32.

See arduino-audio-tools/examples/examples-communication/usb

Unlike the STM32F411 Discovery, this board has no on-board IMU (gyro/accelerometer/magnetometer).
Instead it has an on-board 1.54" 240x240 TFT display (ST7789H2 controller, connected via FMC) -
see the `screen` example. The panel's bus (16-bit FMC) isn't supported by any of the popular
Arduino display libraries, so the sketch drives it directly via HAL, but wraps that as an
`Adafruit_GFX` subclass (install "Adafruit GFX Library" from the Library Manager) so fonts,
text layout and shapes come from that library instead of being reimplemented.

The audio codec on this board is a WM8994 (not the CS43L22 used on the F411 Discovery), connected
via SAI2 instead of the classic I2S peripheral. The `audio-out` example is provided as a template,
but getting it fully working still requires SAI2 support in the low-level `stm32-i2s` backend
(https://github.com/pschatzmann/stm32f411-i2s) and matching `PeripheralPins.c` entries for this
board's Arduino core variant.
