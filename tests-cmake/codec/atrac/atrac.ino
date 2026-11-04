/**
 * @file atrac.ino
 * @brief Decodes ATRAC1, ATRAC3, ATRAC3+ and ATRAC9 streams from the WAV, OMA
 * and AEA containers and compares the result with the output of FFmpeg. The
 * containers are assembled from the sample data of the codec-atrac library.
 * The sketch ends with exit code 0 if all checks have passed.
 *
 * @copyright Copyright (c) 2026
 */
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/AudioCodecs/ContainerATRAC.h"

typedef std::vector<uint8_t> Bytes;

/// Collects the decoded PCM data
class CollectPrint : public Print {
 public:
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    pcm.insert(pcm.end(), data, data + len);
    return len;
  }
  Bytes pcm;
};

int failed = 0;
#define CHECK(cond)                                      \
  do {                                                   \
    if (!(cond)) {                                       \
      failed++;                                          \
      printf("  FAILED %d: %s\n", __LINE__, #cond);      \
    }                                                    \
  } while (0)

Bytes readFile(const char* name) {
  Bytes b;
  FILE* f = fopen((std::string(TEST_DATA_DIR) + "/" + name).c_str(), "rb");
  if (!f) return b;
  uint8_t buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) b.insert(b.end(), buf, buf + n);
  fclose(f);
  return b;
}

void put(Bytes& b, const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  b.insert(b.end(), p, p + len);
}
void put(Bytes& b, const char* s) { put(b, s, strlen(s)); }
void le16(Bytes& b, uint32_t v) { b.push_back(v); b.push_back(v >> 8); }
void le32(Bytes& b, uint32_t v) { le16(b, v); le16(b, v >> 16); }

/// RIFF/WAVE with some extra chunks which must be skipped
Bytes makeWav(const uint8_t* fmt, size_t fmt_len, const uint8_t* data,
              size_t data_len) {
  Bytes b;
  put(b, "RIFF"); le32(b, 0); put(b, "WAVE");
  put(b, "fact"); le32(b, 3); put(b, "abc"); b.push_back(0);  // odd size
  put(b, "fmt "); le32(b, fmt_len); put(b, fmt, fmt_len);
  if (fmt_len & 1) b.push_back(0);
  put(b, "LIST"); le32(b, 4); put(b, "INFO");
  put(b, "data"); le32(b, data_len); put(b, data, data_len);
  put(b, "junk"); le32(b, 4); put(b, "tail");  // after the data
  return b;
}

/// ATRAC3+ in WAV (WAVE_FORMAT_EXTENSIBLE without extradata)
Bytes makeAt3pWav(const Bytes& pkts, int block_align) {
  static const uint8_t guid[16] = {0xBF, 0xAA, 0x23, 0xE9, 0x58, 0xCB, 0x71, 0x44,
                                   0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62};
  Bytes fmt;
  le16(fmt, 0xFFFE); le16(fmt, 2); le32(fmt, 44100); le32(fmt, 0);
  le16(fmt, block_align); le16(fmt, 0);
  le16(fmt, 22); le16(fmt, 0); le32(fmt, 3);
  put(fmt, guid, 16);
  return makeWav(fmt.data(), fmt.size(), pkts.data(), pkts.size());
}

Bytes makeAt9Wav(const Bytes& extra, const Bytes& pkts, int rate, int channels,
                 int block_align) {
  static const uint8_t guid[16] = {0xD2, 0x42, 0xE1, 0x47, 0xBA, 0x36, 0x8D, 0x4D,
                                   0x88, 0xFC, 0x61, 0x65, 0x4F, 0x8C, 0x83, 0x6C};
  Bytes fmt;
  le16(fmt, 0xFFFE); le16(fmt, channels); le32(fmt, rate); le32(fmt, 0);
  le16(fmt, block_align); le16(fmt, 0);
  le16(fmt, 22 + extra.size()); le16(fmt, 0); le32(fmt, 3);
  put(fmt, guid, 16); put(fmt, extra.data(), extra.size());
  return makeWav(fmt.data(), fmt.size(), pkts.data(), pkts.size());
}

/// OMA: 'ea3' tag with some padding + EA3 header + frames
Bytes makeOma(int codec, uint32_t params, const uint8_t* data, size_t len) {
  Bytes b;
  put(b, "ea3"); b.push_back(3); b.push_back(0); b.push_back(0);
  b.push_back(0); b.push_back(0); b.push_back(0); b.push_back(20);  // tag size
  b.resize(b.size() + 20);
  Bytes h(96, 0);
  h[0] = 'E'; h[1] = 'A'; h[2] = '3'; h[3] = 2; h[5] = 96;
  h[6] = 0xFF; h[7] = 0xFF;
  h[32] = codec;
  h[33] = params >> 16; h[34] = params >> 8; h[35] = params;
  put(b, h.data(), h.size());
  put(b, data, len);
  return b;
}

Bytes makeAea(int channels, const Bytes& pkts) {
  Bytes b(2048, 0);
  b[1] = 8;
  b[264] = channels;
  put(b, pkts.data(), pkts.size());
  return b;
}

/// Writes the stream in chunks of different sizes
Bytes decode(AudioDecoder& dec, const Bytes& in, size_t chunk) {
  CollectPrint out;
  dec.setOutput(out);
  dec.begin();
  for (size_t pos = 0; pos < in.size(); pos += chunk) {
    size_t n = in.size() - pos < chunk ? in.size() - pos : chunk;
    dec.write(in.data() + pos, n);
  }
  dec.end();
  return out.pcm;
}

/// compares with the FFmpeg output (the reference is only a prefix)
void compare(const char* name, const Bytes& pcm, const char* ref_name,
             int tolerance) {
  Bytes ref = readFile(ref_name);
  CHECK(!ref.empty());
  CHECK(pcm.size() >= ref.size());
  int max_diff = 0;
  size_t n = pcm.size() < ref.size() ? pcm.size() : ref.size();
  for (size_t i = 0; i + 1 < n; i += 2) {
    int a = (int16_t)(pcm[i] | (pcm[i + 1] << 8));
    int b = (int16_t)(ref[i] | (ref[i + 1] << 8));
    int d = a > b ? a - b : b - a;
    if (d > max_diff) max_diff = d;
  }
  printf("%s: %d bytes, max diff %d\n", name, (int)pcm.size(), max_diff);
  CHECK(max_diff <= tolerance);
}

void testAll() {
  const size_t chunks[] = {1, 7, 1000, 100000};

  // ATRAC3 in WAV
  Bytes wav = readFile("atrac3_stereo_66k.wav");
  CHECK(wav.size() > 100);
  DecoderATRAC atrac3(ATRAC3), atrac3plus(ATRAC3PLUS), atrac9(ATRAC9);
  for (size_t chunk : chunks) {
    WAVDecoder dec;
    dec.addDecoder(atrac3);
    dec.addDecoder(atrac3plus);
    dec.addDecoder(atrac9);
    compare("ATRAC3 wav", decode(dec, wav, chunk), "atrac3_stereo_66k.ref.s16", 16);
    CHECK(dec.audioInfo().sample_rate == 44100 && dec.audioInfo().channels == 2);
    CHECK(dec.getHeader().extraData().size() == 14);
  }

  // ATRAC3 in OMA: 192 bytes / frame, joint stereo
  const uint8_t* frames = wav.data() + 0x3c;
  size_t frames_len = 0x8f40;
  Bytes oma = makeOma(0, (1 << 17) | (1 << 13) | (192 / 8), frames, frames_len);
  for (size_t chunk : chunks) {
    ContainerATRACOMA dec;
    compare("ATRAC3 oma", decode(dec, oma, chunk), "atrac3_stereo_66k.ref.s16", 16);
  }

  // ATRAC3+ in OMA: 1488 bytes / frame, stereo
  Bytes at3p = readFile("atrac3plus_short.pkts");
  Bytes oma3p = makeOma(1, (1 << 13) | (2 << 10) | ((1488 - 8) / 8), at3p.data(), at3p.size());
  for (size_t chunk : chunks) {
    ContainerATRACOMA dec;
    compare("ATRAC3+ oma", decode(dec, oma3p, chunk), "atrac3plus_short.ref.s16", 0);
  }

  // ATRAC3+ in WAV
  Bytes wav3p = makeAt3pWav(at3p, 1488);
  for (size_t chunk : chunks) {
    WAVDecoder dec;
    dec.addDecoder(atrac3);
    dec.addDecoder(atrac3plus);
    dec.addDecoder(atrac9);
    compare("ATRAC3+ wav", decode(dec, wav3p, chunk), "atrac3plus_short.ref.s16", 0);
  }

  // ATRAC1 in AEA
  Bytes at1 = readFile("atrac1_chirp.pkts");
  Bytes aea = makeAea(2, at1);
  for (size_t chunk : chunks) {
    ContainerATRACAEA dec;
    compare("ATRAC1 aea", decode(dec, aea, chunk), "atrac1_chirp.ref.s16", 0);
  }

  // ATRAC9 in WAV
  Bytes extra = readFile("atrac9_stereo.extradata");
  Bytes at9 = readFile("atrac9_stereo.pkts");
  Bytes wav9 = makeAt9Wav(extra, at9, 48000, 2, 512);
  for (size_t chunk : chunks) {
    WAVDecoder dec;
    dec.addDecoder(atrac3);
    dec.addDecoder(atrac3plus);
    dec.addDecoder(atrac9);
    compare("ATRAC9 wav", decode(dec, wav9, chunk), "atrac9_stereo.ref.s16", 0);
  }

  // invalid data is rejected
  Bytes garbage(3000, 0x55);
  ContainerATRACOMA o;
  ContainerATRACAEA a;
  ContainerATRAC* containers[] = {&o, &a};
  for (ContainerATRAC* c : containers) {
    CollectPrint out;
    c->setOutput(out);
    c->begin();
    CHECK(c->write(garbage.data(), garbage.size()) == 0);
    CHECK(!*c);
    CHECK(out.pcm.empty());
    c->end();
  }
}

void setup() {
  testAll();
  printf(failed == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED: %d\n", failed);
  exit(failed == 0 ? 0 : 1);
}

void loop() {}
