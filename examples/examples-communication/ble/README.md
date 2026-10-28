# Audio via BLE Serial

These sketches send ADPCM encoded audio with `BLESerialServer` and
`BLESerialClient` (see [BLESerial](../../../src/AudioTools/Communication/BLESerial/README.md)).
They work on the ESP32 (ESP32 BLE library) and on boards that are supported by
[ArduinoBLE](https://github.com/arduino-libraries/ArduinoBLE).

- `server-as-source`: the server sends the audio and the client reads it. The
  receiving client controls the speed, which works well.
- `client-as-source`: the client writes the audio to the server. The
  throughput in this direction is very limited.

Always start the matching pair of sketches. You also need the
[ADPCM library](https://github.com/pschatzmann/adpcm).
