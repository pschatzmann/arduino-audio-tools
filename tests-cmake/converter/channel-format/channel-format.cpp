// ChannelFormatConverterStream as output (constructed with a Print) and as
// input (constructed with a Stream): 2 -> 1 channels averages the channels.
#include <cassert>
#include <cstdint>
#include <cstdio>

#include "AudioTools.h"

using namespace audio_tools;

/// collects what is written to it
class Collect : public Print {
 public:
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      uint8_t v = data[i];
      bytes.push_back(v);
    }
    return len;
  }
  const int16_t* samples() { return (const int16_t*)bytes.data(); }
  size_t count() { return bytes.size() / sizeof(int16_t); }
  Vector<uint8_t> bytes;
};

static const int16_t stereo[] = {100, 300, -200, -400, 1000, 0, 0, 2000};
static const int16_t mono[] = {200, -300, 500, 1000};

static void test_output() {
  Collect out;
  ChannelFormatConverterStream conv(out);
  assert(conv.begin(AudioInfo(16000, 2, 16), 1));
  size_t written = conv.write((const uint8_t*)stereo, sizeof(stereo));
  assert(written == sizeof(stereo));
  assert(out.count() == 4);
  for (int i = 0; i < 4; i++) assert(out.samples()[i] == mono[i]);
}

static void test_input() {
  MemoryStream in((const uint8_t*)stereo, sizeof(stereo));
  in.begin();
  ChannelFormatConverterStream conv(in);
  assert(conv.begin(AudioInfo(16000, 2, 16), 1));
  int16_t result[4] = {};
  size_t read = conv.readBytes((uint8_t*)result, sizeof(result));
  assert(read == sizeof(result));
  for (int i = 0; i < 4; i++) assert(result[i] == mono[i]);
}

static void test_without_stream() {
  ChannelFormatConverterStream conv;
  assert(!conv.begin(AudioInfo(16000, 2, 16), 1));  // nothing to read or write
  assert(conv.write((const uint8_t*)stereo, sizeof(stereo)) == 0);
  int16_t result[4];
  assert(conv.readBytes((uint8_t*)result, sizeof(result)) == 0);
  assert(conv.available() == 0 && conv.availableForWrite() == 0);
}

int main() {
  test_output();
  test_input();
  test_without_stream();
  printf("channel-format: all tests passed\n");
  return 0;
}
