# Webserver with ADPCM Encoding

This is a variant of [streams-generator-webserver_wav](../streams-generator-webserver_wav) that encodes the generated sine wave as IMA/DVI ADPCM instead of plain PCM before serving it as a WAV stream. Since ADPCM only needs 4 bits per sample instead of 16, this reduces the required bandwidth to about a quarter - useful e.g. when streaming over a slow or congested WiFi connection.

You can register any `AudioEncoderExt` based ADPCM encoder (MS, IMA/DVI, Yamaha) with the `WAVEncoder` that is used internally by `AudioWAVServer`:

```cpp
server.wavEncoder().setEncoder(adpcmEncoder, AudioFormat::DVI_ADPCM);
```

This also automatically writes the extended `fmt `/`fact` chunks that ADPCM WAV files need (equivalent to calling `setExtADPCMHeader(true)`); call `setExtADPCMHeader(false)` afterward if you explicitly want the plain 16-byte header instead.

## Note: this will not play in Chrome (or most browsers)

Unlike the plain PCM [streams-generator-webserver_wav](../streams-generator-webserver_wav) example, this one will **not** produce sound when opened directly in Chrome or most other browsers: their built-in WAV decoder only understands plain PCM (and IEEE float/A-law/µ-law) - not ADPCM-compressed WAV data. To actually hear the stream, use a client capable of decoding ADPCM instead, e.g.:

- `ffplay http://<esp32-ip>/`
- VLC, pointed at the URL
- An ESP32/AudioTools client using `ADPCMDecoder` to decode the stream (the actual use case for the bandwidth savings)
