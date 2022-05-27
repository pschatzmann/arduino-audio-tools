# I2S input and I2S Output with One Port

We use the same I2S Port to input data from an I2S device and output data to another I2S Device. This means that the WS and BCK pins are shared:

| In   | ESP32 | Out  |
|------|-------|------|
| WS   | G14   | WS   |
| BCK  | G15   | BCK  |
| DATA | G19   | -    |
| -    | G18   | DATA |
| VIN  | VIN   | VIN  |
| GND  | GND   | GND  |
