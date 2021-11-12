
#include <Wire.h>
#include "ADS1X15.h"

ADS1115 ads1015(0x48); // ads1015 device  


void list(bool print){
    int count;
    unsigned long end = millis()+1000*10;
    while(end>millis()) {
        int16_t value = ads1015.getValue();;
        if (print) Serial.println(value);
        count++;
    }
    Serial.print("Samples per second: ");
    Serial.println(count/10);
}

void setup(){
    Serial.begin(119200);

      // setup gain for ads1015
    Wire.setClock(400000);
    ads1015.begin();
    if(!ads1015.isConnected()) Serial.println("ads1015 NOT CONNECTED!");
    ads1015.setGain(4);        // 6.144 volt
    ads1015.setDataRate(4);    // 0 = slow   4 = medium   7 = fast  (7 = fails )
    ads1015.setMode(0);
    ads1015.requestADC_Differential_0_1();

    list(false);
    //list(true);
}


void loop(){

}