/**
 * Generic main for desktop arduino emulation
*/
#ifndef NO_MAIN

#pragma once
void loop();
void setup();

int main (void) { 
    setup();
    while(true){
        loop();
    }
 }

#endif