// Functional parity check between the float and PREFER_FIXEDPOINT paths of
// Boost, Fuzz, Tremolo, Delay, ADSRGain and Compressor (AudioEffect.h). Built
// twice (see CMakeLists.txt) with PREFER_FIXEDPOINT=0 and PREFER_FIXEDPOINT=1
// from this same source; stdout is diffed externally to confirm the
// fixed-point path stays close to the float reference.
#include <cstdio>

#include "AudioTools.h"
#include "AudioTools/CoreAudio/AudioEffects/AudioEffect.h"

using namespace audio_tools;

// deterministic 256-sample sine table (same data used by tests-cmake/resample)
extern int16_t arsineC256[256];
int16_t arsineC256[] = { 436, 657, 877, 1096, 1316, 1534, 1751, 1967, 2183, 2396, 2609, 2819, 3029, 3236, 3441, 3644, 3845, 4044, 4240, 4434, 4625, 4813, 4999, 5181, 5360, 5536, 5709, 5878, 6043, 6205, 6364, 6518, 6668, 6815, 6957, 7095, 7229, 7359, 7484, 7604, 7720, 7831, 7938, 8040, 8137, 8229, 8316, 8398, 8475, 8547, 8613, 8675, 8731, 8782, 8828, 8868, 8903, 8933, 8957, 8976, 8989, 8997, 9000, 8997, 8989, 8975, 8956, 8932, 8902, 8867, 8826, 8780, 8729, 8672, 8610, 8544, 8471, 8394, 8312, 8225, 8133, 8035, 7933, 7827, 7715, 7599, 7478, 7353, 7223, 7089, 6951, 6808, 6662, 6511, 6357, 6198, 6036, 5870, 5701, 5528, 5352, 5173, 4990, 4805, 4617, 4425, 4232, 4035, 3836, 3635, 3432, 3227, 3019, 2810, 2599, 2387, 2173, 1958, 1742, 1524, 1306, 1087, 867, 647, 426, 205, -16, -237, -458, -679, -899, -1118, -1337, -1556, -1773, -1989, -2204, -2418, -2630, -2840, -3049, -3256, -3461, -3664, -3865, -4064, -4260, -4453, -4644, -4832, -5017, -5199, -5378, -5553, -5726, -5894, -6060, -6221, -6379, -6533, -6683, -6829, -6971, -7109, -7242, -7371, -7496, -7616, -7731, -7842, -7948, -8050, -8146, -8238, -8324, -8406, -8482, -8554, -8620, -8681, -8736, -8787, -8832, -8872, -8906, -8936, -8959, -8978, -8990, -8998, -9000, -8997, -8988, -8974, -8954, -8929, -8898, -8863, -8822, -8775, -8723, -8666, -8604, -8537, -8464, -8386, -8304, -8216, -8123, -8025, -7923, -7816, -7704, -7587, -7466, -7340, -7210, -7076, -6937, -6794, -6647, -6496, -6341, -6182, -6020, -5854, -5684, -5511, -5334, -5155, -4972, -4786, -4598, -4406, -4212, -4016, -3817, -3615, -3412, -3206, -2999, -2789, -2578, -2366, -2152, -1936, -1720, -1502, -1284, -1065, -845, -625, -404, -183, 38, 180 };

template <typename Effect>
void runEffect(const char* name, Effect& fx) {
  for (int i = 0; i < 256; i++) {
    effect_t out = fx.process(arsineC256[i]);
    printf("%s,%d,%d\n", name, i, (int)out);
  }
}

void setup(void) {
  Boost boost(1.5f);
  runEffect("Boost", boost);

  Fuzz fuzz(6.5f, 300);
  runEffect("Fuzz", fuzz);

  Tremolo tremolo(2000, 50, 44100);
  runEffect("Tremolo", tremolo);

  Delay delay(1000, 0.5f, 0.6f, 44100);
  runEffect("Delay", delay);

  ADSRGain adsr(0.01f, 0.01f, 0.5f, 0.02f, 1.0f);
  adsr.keyOn();
  runEffect("ADSRGain", adsr);

  Compressor comp(44100, 30, 20, 10, 10, 0.5f);
  runEffect("Compressor", comp);

  // longer run (~20 cycles through attack/hold/release) to check the
  // fixed-point state machine stays bounded and doesn't get stuck
  Compressor comp2(44100, 30, 20, 10, 10, 0.5f);
  for (int cycle = 0; cycle < 20; cycle++) {
    for (int i = 0; i < 256; i++) {
      // alternate loud/quiet halves to exercise attack and release
      int16_t sample = (cycle % 2 == 0) ? arsineC256[i] * 3 : arsineC256[i] / 8;
      effect_t out = comp2.process(sample);
      printf("CompressorLong,%d,%d\n", cycle * 256 + i, (int)out);
    }
  }
}

void loop() { exit(0); }
