// Functional parity check between the float and PREFER_FIXEDPOINT paths of
// ConverterScaler, ConverterAutoCenterT, ChannelMixer and ChannelAvgT
// (BaseConverter.h). Built twice (see CMakeLists.txt) with
// PREFER_FIXEDPOINT=0 and PREFER_FIXEDPOINT=1 from this same source; stdout
// is diffed externally to confirm the fixed-point path stays close to the
// float reference.
#include <cstdio>
#include <cstring>

#include "AudioTools.h"
#include "AudioTools/CoreAudio/BaseConverter.h"

using namespace audio_tools;

// deterministic 256-sample sine table (same data used by tests-cmake/resample)
int16_t arsineC256[] = { 436, 657, 877, 1096, 1316, 1534, 1751, 1967, 2183, 2396, 2609, 2819, 3029, 3236, 3441, 3644, 3845, 4044, 4240, 4434, 4625, 4813, 4999, 5181, 5360, 5536, 5709, 5878, 6043, 6205, 6364, 6518, 6668, 6815, 6957, 7095, 7229, 7359, 7484, 7604, 7720, 7831, 7938, 8040, 8137, 8229, 8316, 8398, 8475, 8547, 8613, 8675, 8731, 8782, 8828, 8868, 8903, 8933, 8957, 8976, 8989, 8997, 9000, 8997, 8989, 8975, 8956, 8932, 8902, 8867, 8826, 8780, 8729, 8672, 8610, 8544, 8471, 8394, 8312, 8225, 8133, 8035, 7933, 7827, 7715, 7599, 7478, 7353, 7223, 7089, 6951, 6808, 6662, 6511, 6357, 6198, 6036, 5870, 5701, 5528, 5352, 5173, 4990, 4805, 4617, 4425, 4232, 4035, 3836, 3635, 3432, 3227, 3019, 2810, 2599, 2387, 2173, 1958, 1742, 1524, 1306, 1087, 867, 647, 426, 205, -16, -237, -458, -679, -899, -1118, -1337, -1556, -1773, -1989, -2204, -2418, -2630, -2840, -3049, -3256, -3461, -3664, -3865, -4064, -4260, -4453, -4644, -4832, -5017, -5199, -5378, -5553, -5726, -5894, -6060, -6221, -6379, -6533, -6683, -6829, -6971, -7109, -7242, -7371, -7496, -7616, -7731, -7842, -7948, -8050, -8146, -8238, -8324, -8406, -8482, -8554, -8620, -8681, -8736, -8787, -8832, -8872, -8906, -8936, -8959, -8978, -8990, -8998, -9000, -8997, -8988, -8974, -8954, -8929, -8898, -8863, -8822, -8775, -8723, -8666, -8604, -8537, -8464, -8386, -8304, -8216, -8123, -8025, -7923, -7816, -7704, -7587, -7466, -7340, -7210, -7076, -6937, -6794, -6647, -6496, -6341, -6182, -6020, -5854, -5684, -5511, -5334, -5155, -4972, -4786, -4598, -4406, -4212, -4016, -3817, -3615, -3412, -3206, -2999, -2789, -2578, -2366, -2152, -1936, -1720, -1502, -1284, -1065, -845, -625, -404, -183, 38, 180 };
constexpr int N = 256;

void printBuf(const char* name, int16_t* buf, int n) {
  for (int i = 0; i < n; i++) printf("%s,%d,%d\n", name, i, (int)buf[i]);
}

void setup(void) {
  // ConverterScaler: factor 1.7, offset -500, maxValue 30000, mono
  {
    int16_t buf[N];
    memcpy(buf, arsineC256, sizeof(buf));
    ConverterScaler<int16_t> conv(1.7f, -500, 30000, 1);
    conv.convert((uint8_t*)buf, sizeof(buf));
    printBuf("ConverterScaler", buf, N);
  }

  // ConverterAutoCenterT, static mode, stereo interleaved (duplicate the
  // mono table into both channels, offset one channel to force a DC bias)
  {
    int16_t buf[N * 2];
    for (int i = 0; i < N; i++) {
      buf[i * 2] = arsineC256[i];
      buf[i * 2 + 1] = (int16_t)(arsineC256[i] + 1000);
    }
    ConverterAutoCenterT<int16_t> conv(2, false);
    conv.convert((uint8_t*)buf, sizeof(buf));
    printBuf("ConverterAutoCenterStatic", buf, N * 2);
  }

  // ConverterAutoCenterT, dynamic mode, run over two consecutive blocks
  {
    int16_t buf[N * 2];
    for (int i = 0; i < N; i++) {
      buf[i * 2] = arsineC256[i];
      buf[i * 2 + 1] = (int16_t)(arsineC256[i] + 1000);
    }
    ConverterAutoCenterT<int16_t> conv(2, true);
    conv.convert((uint8_t*)buf, sizeof(buf));
    printBuf("ConverterAutoCenterDynamic1", buf, N * 2);
    for (int i = 0; i < N; i++) {
      buf[i * 2] = (int16_t)(arsineC256[i] / 2);
      buf[i * 2 + 1] = (int16_t)(arsineC256[i] / 2 + 2000);
    }
    conv.convert((uint8_t*)buf, sizeof(buf));
    printBuf("ConverterAutoCenterDynamic2", buf, N * 2);
  }

  // ChannelMixer: 4-channel -> mixed down to 4 identical avg channels
  {
    int16_t buf[N * 4];
    for (int i = 0; i < N; i++) {
      buf[i * 4 + 0] = arsineC256[i];
      buf[i * 4 + 1] = (int16_t)(arsineC256[i] / 2);
      buf[i * 4 + 2] = (int16_t)(-arsineC256[i] / 3);
      buf[i * 4 + 3] = (int16_t)(arsineC256[i] / 4);
    }
    ChannelMixer<int16_t> mix(4);
    mix.convert((uint8_t*)buf, sizeof(buf));
    printBuf("ChannelMixer", buf, N * 4);
  }

  // ChannelAvg (dispatches to ChannelAvgT<int16_t, ...>) on stereo data
  {
    int16_t src[N * 2];
    int16_t dst[N];
    for (int i = 0; i < N; i++) {
      src[i * 2] = arsineC256[i];
      src[i * 2 + 1] = (int16_t)(-arsineC256[i] / 2);
    }
    ChannelAvg avg(16);
    avg.convert((uint8_t*)dst, (uint8_t*)src, sizeof(src));
    printBuf("ChannelAvg", dst, N);
  }

  // compile + smoke-run coverage for non-int16_t T (int24_t routes samples
  // through soft_float_t/float differently than primitive types, since it's
  // a class type -- this exercises that path for ConverterScaler,
  // ConverterAutoCenterT and ChannelMixer)
  {
    int24_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = arsineC256[i];
    ConverterScaler<int24_t> conv(1.2f, -100, 8000000, 1);
    conv.convert((uint8_t*)buf, sizeof(buf));
    for (int i = 0; i < 8; i++) printf("ConverterScaler24,%d,%d\n", i, (int)(int32_t)buf[i]);
  }
  {
    int24_t buf[16];
    for (int i = 0; i < 8; i++) {
      buf[i * 2] = arsineC256[i];
      buf[i * 2 + 1] = (int32_t)arsineC256[i] + 1000;
    }
    ConverterAutoCenterT<int24_t> conv(2, false);
    conv.convert((uint8_t*)buf, sizeof(buf));
    for (int i = 0; i < 16; i++) printf("ConverterAutoCenter24,%d,%d\n", i, (int)(int32_t)buf[i]);
  }
  {
    int24_t buf[16];
    for (int i = 0; i < 8; i++) {
      buf[i * 2] = arsineC256[i];
      buf[i * 2 + 1] = (int32_t)(-arsineC256[i] / 2);
    }
    ChannelMixer<int24_t> mix(2);
    mix.convert((uint8_t*)buf, sizeof(buf));
    for (int i = 0; i < 16; i++) printf("ChannelMixer24,%d,%d\n", i, (int)(int32_t)buf[i]);
  }
  {
    int8_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = (int8_t)(arsineC256[i] / 128);
    ConverterScaler<int8_t> conv(1.3f, -5, 100, 1);
    conv.convert((uint8_t*)buf, sizeof(buf));
    for (int i = 0; i < 8; i++) printf("ConverterScaler8,%d,%d\n", i, (int)buf[i]);
  }
}

void loop() { exit(0); }
