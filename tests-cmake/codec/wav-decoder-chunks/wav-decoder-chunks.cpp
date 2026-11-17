// WAVDecoder must output all PCM data, independent of how the input is split
// into write() calls (regression: the data that arrived in the same write as
// the header was dropped), and must not decode bytes after the declared 'data'
// chunk (e.g. a trailing LIST chunk). Exit code 0 = pass.
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

// builds a 1 s, 16 kHz mono WAV file in memory. If declared_length is set, the
// header contains the exact data length (non-streamed), otherwise it is
// streamed.
static std::vector<uint8_t> buildWav(const std::vector<int16_t>& pcm,
                                     bool declared_length) {
  CollectingPrint wav_file;
  WAVEncoder encoder;
  encoder.setOutput(wav_file);
  AudioInfo info(16000, 1, 16);
  if (declared_length) encoder.setDataLength(pcm.size() * 2);
  encoder.begin(info);
  encoder.write((uint8_t*)pcm.data(), pcm.size() * 2);
  return wav_file.data;
}

// returns the number of failed decodings
static int runCase(const char* name, const std::vector<uint8_t>& file,
                   const std::vector<int16_t>& pcm) {
  int failures = 0;
  for (size_t chunk : {1ul, 2ul, 7ul, 44ul, 100ul, 1024ul, 4096ul, file.size()}) {
    CollectingPrint out;
    WAVDecoder decoder;
    decoder.setOutput(out);
    decoder.begin();
    for (size_t p = 0; p < file.size(); p += chunk)
      decoder.write(file.data() + p, std::min(chunk, file.size() - p));
    bool ok = out.data.size() == pcm.size() * 2 &&
              memcmp(out.data.data(), pcm.data(), out.data.size()) == 0;
    printf("%-22s chunk %6zu: %zu bytes %s\n", name, chunk, out.data.size(),
           ok ? "ok" : "FAIL");
    if (!ok) failures++;
  }
  return failures;
}

void setup() {
  std::vector<int16_t> pcm(16000);
  for (size_t j = 0; j < pcm.size(); j++) pcm[j] = (int16_t)(j * 3);

  // trailing non-audio LIST chunk after the data chunk
  std::vector<uint8_t> trailing_list = buildWav(pcm, true);
  const char list_tag[] = "LIST";
  trailing_list.insert(trailing_list.end(), list_tag, list_tag + 4);
  uint32_t list_len = 26;
  for (int i = 0; i < 4; i++)
    trailing_list.push_back((uint8_t)(list_len >> (8 * i)));
  for (uint32_t i = 0; i < list_len; i++) trailing_list.push_back(0x55);

  int failures = 0;
  failures += runCase("declared length", buildWav(pcm, true), pcm);
  failures += runCase("declared + trailing LIST", trailing_list, pcm);
  failures += runCase("streamed", buildWav(pcm, false), pcm);

  printf("%s\n", failures == 0 ? "PASS" : "FAIL");
  exit(failures == 0 ? 0 : 1);
}

void loop() {}
