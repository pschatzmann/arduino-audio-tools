// Simple wrapper for Arduino sketch to compilable with cpp in cmake
#include "Arduino.h"
#include "AudioTools.h"

AudioInfo info(44100, 1, 16);
//SineWaveGenerator<int16_t> sineWave(32000);                // subclass of SoundGenerator with max amplitude of 32000
int16_t arsineC256[] = { 436, 657, 877, 1096, 1316, 1534, 1751, 1967, 2183, 2396, 2609, 2819, 3029, 3236, 3441, 3644, 3845, 4044, 4240, 4434, 4625, 4813, 4999, 5181, 5360, 5536, 5709, 5878, 6043, 6205, 6364, 6518, 6668, 6815, 6957, 7095, 7229, 7359, 7484, 7604, 7720, 7831, 7938, 8040, 8137, 8229, 8316, 8398, 8475, 8547, 8613, 8675, 8731, 8782, 8828, 8868, 8903, 8933, 8957, 8976, 8989, 8997, 9000, 8997, 8989, 8975, 8956, 8932, 8902, 8867, 8826, 8780, 8729, 8672, 8610, 8544, 8471, 8394, 8312, 8225, 8133, 8035, 7933, 7827, 7715, 7599, 7478, 7353, 7223, 7089, 6951, 6808, 6662, 6511, 6357, 6198, 6036, 5870, 5701, 5528, 5352, 5173, 4990, 4805, 4617, 4425, 4232, 4035, 3836, 3635, 3432, 3227, 3019, 2810, 2599, 2387, 2173, 1958, 1742, 1524, 1306, 1087, 867, 647, 426, 205, -16, -237, -458, -679, -899, -1118, -1337, -1556, -1773, -1989, -2204, -2418, -2630, -2840, -3049, -3256, -3461, -3664, -3865, -4064, -4260, -4453, -4644, -4832, -5017, -5199, -5378, -5553, -5726, -5894, -6060, -6221, -6379, -6533, -6683, -6829, -6971, -7109, -7242, -7371, -7496, -7616, -7731, -7842, -7948, -8050, -8146, -8238, -8324, -8406, -8482, -8554, -8620, -8681, -8736, -8787, -8832, -8872, -8906, -8936, -8959, -8978, -8990, -8998, -9000, -8997, -8988, -8974, -8954, -8929, -8898, -8863, -8822, -8775, -8723, -8666, -8604, -8537, -8464, -8386, -8304, -8216, -8123, -8025, -7923, -7816, -7704, -7587, -7466, -7340, -7210, -7076, -6937, -6794, -6647, -6496, -6341, -6182, -6020, -5854, -5684, -5511, -5334, -5155, -4972, -4786, -4598, -4406, -4212, -4016, -3817, -3615, -3412, -3206, -2999, -2789, -2578, -2366, -2152, -1936, -1720, -1502, -1284, -1065, -845, -625, -404, -183, 38, 180 }; 
GeneratorFromArray<int16_t> sineWave(arsineC256,0,false); 
GeneratedSoundStream<int16_t> sound(sineWave);             // Stream generated from sine wave
ResampleStream out(sound);                        // We double the output sample rate
CsvOutput<int16_t> csv(Serial);                  // Output to Serial
InputMerge<int16_t> imerge; 
StreamCopy copier(csv, imerge, 1024);                       // copies sound to out

// Arduino Setup
void setup(void) {  
  // Open Serial 
  //Serial.begin(115200);
  //AudioLogger::instance().begin(Serial, AudioLogger::Info);

  auto config = out.defaultConfig();
  config.copyFrom(info); 
  out.begin(config);

  // Define CSV Output
  csv.begin(info);
  sound.begin(info);
//  sineWave.begin(config, N_B4);
  sineWave.begin(info);
  imerge.begin(info);
  imerge.add(out);
  //Serial.println("started (mixer)...");
}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
  //Serial.println("----");
}
