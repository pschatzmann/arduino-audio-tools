
#include "AudioTools.h"

TimerCallbackAudioStream timerStream;

// Reads the data in the callback driven by a timer
uint16_t IRAM_ATTR callback(uint8_t *data, uint16_t len){
    for (int j=0;j<len;j++){
        assert(data[j]==1);
    }
    return len;
}


void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial,AudioLogger::Info);

    auto cfg = timerStream.defaultConfig();
    cfg.rx_tx_mode = TX_MODE;
    cfg.channels = 1;
    cfg.sample_rate = 5000;
    cfg.bits_per_sample = 16;
    cfg.callback = callback;
    timerStream.begin(cfg);
}


// write to the TimerCallbackAudioStream 
void loop(){
    uint8_t buffer[512];
    memset(buffer,1,512);
    while (true){
        int len = timerStream.write(buffer, 512);
        Serial.println(len);
    }
}