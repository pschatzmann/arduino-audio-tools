// The module comes with a MSM261 I2S Microphone
// Make sure that USB CDC on Boot is enabled


const int I2S_WS = 37;
const int I2S_SCK = 36;
const int I2S_SD = 35;
const AudioInfo info(8000, 1, 16);
I2SStream i2sStream; // Access I2S as stream
CsvOutput<int16_t> csvStream(Serial);
StreamCopy copier(csvStream, i2sStream); // copy i2sStream to csvStream

// Arduino Setup
void setup(void) {
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial, AudioLogger::Info);
    
    auto cfg = i2sStream.defaultConfig(RX_MODE);
    cfg.copyFrom(info);
    cfg.i2s_format = I2S_STD_FORMAT; // or try with I2S_LSB_FORMAT
    cfg.use_apll = false;  
    cfg.pin_data = I2S_SD; 
    cfg.pin_mck = I2S_SCK; 
    cfg.pin_ws = I2S_WS; 
    i2sStream.begin(cfg);

    // make sure that we have the correct channels set up
    csvStream.begin(info);

    Serial.println("starting...");

}

// Arduino loop - copy data
void loop() {
    copier.copy();
}