// WAVDecoder must output all PCM data, independent of how the input is split
// into write() calls (regression: the data that arrived in the same write as
// the header was dropped). Exit code 0 = pass.
#include <vector>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"

struct CollectingPrint : public Print {
  std::vector<uint8_t> data;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    data.insert(data.end(), b, b + n);
    return n;
  }
};

void setup() {
  // build a WAV file in memory with WAVEncoder: 1 s, 16 kHz mono, ramp
  CollectingPrint wav_file;
  WAVEncoder encoder;
  encoder.setOutput(wav_file);
  AudioInfo info(16000, 1, 16);
  encoder.begin(info);
  std::vector<int16_t> pcm(16000);
  for (size_t j = 0; j < pcm.size(); j++) pcm[j] = (int16_t)(j * 3);
  encoder.write((uint8_t*)pcm.data(), pcm.size() * 2);
  const std::vector<uint8_t>& file = wav_file.data;

  int failures = 0;
  for (size_t chunk : {1ul, 44ul, 100ul, 1024ul, 4096ul, file.size()}) {
    CollectingPrint out;
    WAVDecoder decoder;
    decoder.setOutput(out);
    decoder.begin();
    for (size_t p = 0; p < file.size(); p += chunk)
      decoder.write(file.data() + p, std::min(chunk, file.size() - p));
    bool ok = out.data.size() == pcm.size() * 2 &&
              memcmp(out.data.data(), pcm.data(), out.data.size()) == 0;
    printf("chunk %6zu: %zu bytes %s\n", chunk, out.data.size(), ok ? "ok" : "FAIL");
    if (!ok) failures++;
  }
  printf("%s\n", failures == 0 ? "PASS" : "FAIL");
  exit(failures == 0 ? 0 : 1);
}

void loop() {}
