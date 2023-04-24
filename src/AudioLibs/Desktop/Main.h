/**
 * Generic main for desktop arduino emulation
*/
#pragma once

void loop();
void setup();

int main (void) { 
    setup();
    while(true){
        loop();
    }
 }	