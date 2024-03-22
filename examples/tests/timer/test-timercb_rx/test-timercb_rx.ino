
#include "AudioTools.h"

TimerCallbackAudioStream timerStream;

uint16_t IRAM_ATTR callback(uint8_t *data, uint16_t len){
    uint16_t result = len > 256 ? 256 : len;
   
    for (uint16_t j=0;j<result;j++){
        data[j]=1;
    }
    return result;
}


void setup(){
    Serial.begin(115200);
    AudioLogger::instance().begin(Serial,AudioLogger::Info);

    auto cfg = timerStream.defaultConfig();
    cfg.rx_tx_mode = RX_MODE;
    cfg.channels = 1;
    cfg.sample_rate = 5000;
    cfg.bits_per_sample = 16;
    cfg.callback = callback;
    timerStream.begin(cfg);

}

/// read from the TimerCallbackAudioStream
void loop(){
    uint8_t buffer[512];
    while (true){
        int len = timerStream.readBytes(buffer, 512);
        for (int j=0;j<len;j++){
            assert(buffer[j]==1);
        }
        Serial.println(len);
        yield();
    }
}