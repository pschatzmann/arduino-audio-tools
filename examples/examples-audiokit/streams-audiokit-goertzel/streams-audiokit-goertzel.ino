// Example for DTMF detection using Goertzel algorithm
// Uses AudioKit and I2S microphones as input

#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"

AudioInfo info(44100, 2, 16);             // 8kHz, mono, 16 bits
AudioBoardStream kit(AudioKitEs8388V1);  // Access I2S as stream
//CsvOutput<int16_t> out(Serial, 1);
GoertzelStream goerzel; //(out);
StreamCopy copier(goerzel, kit);  // copy kit to georzel

// represent DTMF keys
class DTMF {
 public:
  enum Dimension { Row, Col };
  DTMF() = default;
  DTMF(Dimension d, int i) {
    if (d == Row)
      row = i;
    else
      col = i;
  }
  void clear() {
    row = -1;
    col = -1;
  }
  char getChar() {
    if (row == -1 || col == -1) return '?';
    return keys[row][col];
  }
  int row = -1;
  int col = -1;
  const char* keys[4] = {"123A", "456B", "789C", "*0#D"};
} actual_dtmf;

// combine row and col information
void GoezelCallback(float frequency, float magnitude, void* ref) {
  DTMF* dtmf = (DTMF*)ref;
  LOGW("Time: %lu - Hz: %f  Mag: %f", millis(), frequency, magnitude);
  // we get either row or col information
  if (dtmf->row != -1) {
    actual_dtmf.row = dtmf->row;
  } else {
    actual_dtmf.col = dtmf->col;

    // print detected key
    Serial.println(actual_dtmf.getChar());
    actual_dtmf.clear();
  }
}

void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // start audio input from microphones
  auto cfg = kit.defaultConfig(RX_MODE);
  cfg.copyFrom(info);
  cfg.sd_active = false;
  cfg.input_device = ADC_INPUT_LINE2;
  cfg.use_apll = false;
  kit.begin(cfg);

  // lower frequencies - with keys
  goerzel.addFrequency(697, new DTMF(DTMF::Row, 0));
  goerzel.addFrequency(770, new DTMF(DTMF::Row, 1));
  goerzel.addFrequency(852, new DTMF(DTMF::Row, 2));
  goerzel.addFrequency(941, new DTMF(DTMF::Row, 3));
  // higher frequencies with idx
  goerzel.addFrequency(1209, new DTMF(DTMF::Col, 0));
  goerzel.addFrequency(1336, new DTMF(DTMF::Col, 1));
  goerzel.addFrequency(1477, new DTMF(DTMF::Col, 2));
  goerzel.addFrequency(1633, new DTMF(DTMF::Col, 3));
  // define callback
  goerzel.setFrequencyDetectionCallback(GoezelCallback);

  // start goerzel
  auto gcfg = goerzel.defaultConfig();
  gcfg.copyFrom(info);
  gcfg.threshold = 5.0;
  gcfg.block_size = 1024;
  goerzel.begin(gcfg);
}

void loop() { copier.copy(); }
