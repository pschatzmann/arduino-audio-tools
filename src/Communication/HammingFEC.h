#pragma once
#include "AudioConfig.h"
#include "AudioTools/BaseStream.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

namespace audio_tools {

/**
 * @brief Hamming forware error correction. Inspired by
 * https://github.com/nasserkessas/hamming-codes
 * 
 * Hamming<1024,uint16_t> hamming; // block_ts of 1k with block_tsize 16bits = 31.25% redundency
 * 
 * Block size (bits)	Redundant bits	Redundancy percentage
 * 4	3	75%
 * 8	4	50%
 * 16	5	31.25%
 * 32	6	18.75%
 * 64	7	10.94%
 * 
 * @ingroup fec
*/

template <int bytecount, class block_t>
class HammingFEC : public BaseStream {
  public:
    HammingFEC(Stream &stream){
        p_stream = &stream;
        p_print = &stream;
    }

    HammingFEC(Print &print){
        p_print = &print;
    }

    int availableForWrite() override {
        return bytecount;
    }
    
    size_t write(const uint8_t* data, size_t len)override{
        if (p_print==nullptr) return 0;
        for (int j=0;j<len;j++){
            raw.write(data[j]);
            if(raw.availableForWrite()==0){
                encode(raw.data(), raw.size());
                raw.reset();
            }
        }
        return len;
    }

    int available() override{
        return bytecount;
    }

    size_t readBytes(uint8_t* data, size_t len)override{
        if (p_stream==nullptr) return 0;
        if (encoded.isEmpty()){
            // read encoded data from surce
            p_stream->readBytes(encoded.data(), encodedSize());
            encoded.setAvailable(encodedSize());
            // decode data
            if(decode((block_t*)encoded.data(), bytecount)){
                raw.setAvailable(bytecount);
            }
        }
        return raw.readArray(data, len);
    }

  protected:
    SingleBuffer<uint8_t> raw{bytecount};
    SingleBuffer<uint8_t> encoded{encodedSize()};
    Stream* p_stream = nullptr;
    Print* p_print = nullptr;

    int getBlocks(){
        // Amount of bits in a block_t //
        int bits = sizeof(block_t) * 8;

        // Amount of bits per block_t used to carry the message //
        int messageBits = bits - log2(bits) - 1;

        // Amount of block_ts needed to encode message //
        int block_ts = ceil((float) bytecount / messageBits);

        return block_ts;
    } 

    int encodedSize(){
        return getBlocks()+1*sizeof(block_t);
    }
   
    int encode(uint8_t *input, int len) {

        // Amount of bits in a block_t //
        int bits = sizeof(block_t) * 8;

        // Amount of bits per block_t used to carry the message //
        int messageBits = bits - log2(bits) - 1;

        // Amount of block_ts needed to encode message //
        int block_ts = ceil((float) len / messageBits);

        // Array of encoded block_ts //
        block_t encoded[block_ts+1];

        // Loop through each block_t //
        for (int i = 0; i < block_ts+1; i++) {

            // Final encoded block_t variable //
            block_t thisBlock = 0;
            
            // Amount of "skipped" bits (used for parity) //
            int skipped = 0;

            // Count of how many bits are "on" //
            int onCount = 0;
            
            // Array of "on" bits //
            int onList[bits];

            // Loop through each message bit in this block_t to populate final block_t //
            for (int j = 0; j < bits; j++) {

                // Skip bit if reserved for parity bit //
                if ((j & (j - 1)) == 0) { // Check if j is a power of two or 0
                    skipped++;
                    continue;
                }

                bool thisBit;
                
                if (i != block_ts) {

                    // Current overall bit number //
                    int currentBit = i*messageBits + (j-skipped);
                    
                    // Current uint8_tacter //
                    int currentChar = currentBit/(sizeof(uint8_t)*8); // int division
                    
                    // Value of current bit //
                    thisBit = currentBit < len*sizeof(uint8_t)*8 ? getCharBit(input[currentChar], currentBit-currentChar*8) : 0;
                }

                else {
                    thisBit = getBit(len/8, j-skipped+(sizeof(block_t)*8-messageBits));
                }

            // If bit is "on", add to onList and onCount //
                if (thisBit) {
                    onList[onCount] = j;
                    onCount++;
                }
                
                // Populate final message block_t //
                thisBlock = modifyBit(thisBlock, j, thisBit);
            }

            // Calculate values of parity bits //
            block_t parityBits = multipleXor(onList, onCount);

            // Loop through skipped bits (parity bits) //
            for (int k = 1; k < skipped; k++) { // skip bit 0

                // If bit is "on", add to onCount
                if (getBit(parityBits, sizeof(block_t)*8-skipped+k)) {
                    onCount++;
                }

                // Add parity bit to final block_t //
                thisBlock = modifyBit(thisBlock, (int) pow(2, skipped-k-1) , getBit(parityBits, sizeof(block_t)*8-skipped+k));
            }

            // Add overall parity bit (total parity of onCount) //
            thisBlock = modifyBit(thisBlock, 0, onCount & 1);

            // Add block_t to encoded block_ts //
            encoded[i] = thisBlock;
        }
        
        // Write encoded message to file /
        return p_print->write((const uint8_t*)encoded, sizeof(block_t)*block_ts+1);
    }

    bool decode(block_t input[], int len) {
        uint8_t* output = (uint8_t*)raw.data();
        
        // Amount of bits in a block_t //
        int bits = sizeof(block_t) * 8;

        for (int b = 0; b < (len/sizeof(block_t)); b++) {

            // Count of how many bits are "on" //
            int onCount = 0;
            
            // Array of "on" bits //
            int onList[bits];

            // Populate onCount and onList //
            for (int i = 1; i < bits; i++) {
                getBit(input[b], i);
                if (getBit(input[b], i)) {
                    onList[onCount] = i;
                    onCount++;
                }
            }

            // Check for single errors //
            int errorLoc = multipleXor(onList, onCount);

            if (errorLoc) {
                
                // Check for multiple errors //
                if (!(onCount & 1 ^ getBit(input[b], 0))) { // last bit of onCount (total parity) XOR first bit of block_t (parity bit)
                    LOGE("\nMore than one error detected. Aborting.\n");
                    return false;
                }
                
                // Flip error bit //
                else {
                    input[b] = toggleBit(input[b], (bits-1) - errorLoc);
                                    }
            }
        }

        // Amount of bits per block_t used to carry the message //
        int messageBits = bits - log2(bits) - 1;

        int currentBit, currentChar;

        int uint8_ts = 0;

        for (int i = 0; i < len/sizeof(block_t); i++) {

            // Initialise variable to store amount of parity bits passed //
            int skipped = 0;

            // Loop through each message bit in this block_t to populate final block_t //
            for (int j = 0; j < bits; j++) {

                // Skip bit if reserved for parity bit //
                if ((j & (j - 1)) == 0) { // Check if j is a power of two or 0
                    skipped++;
                    continue;
                }

                // Current overall bit number //
                currentBit = i*messageBits + (j-skipped);

                // Current uint8_tacter //
                currentChar = currentBit/(sizeof(uint8_t)*8); // int division

                // Value of current bit //
                bool thisBit = getBit(input[i], j);

                if (i != len/sizeof(block_t)-1) {
                
                    // Populate final decoded uint8_tacter //
                    output[currentChar] = modifyCharBit(output[currentChar], currentBit-currentChar*sizeof(uint8_t)*8, thisBit);
                }

                else {
                    uint8_ts = modifyBit(uint8_ts, j-skipped+(sizeof(block_t)*8-messageBits), thisBit);
                }

            }
        }


        return uint8_ts;
    }

    int multipleXor(int *indicies, int len) {
        int val = indicies[0];
        for (int i = 1; i < len; i++) {
            val = val ^ indicies[i];
        }
        return val;
    }

    block_t modifyBit(block_t n, int p, bool b) {
        return ((n & ~(1 << (sizeof(block_t)*8-1-p))) | (b << (sizeof(block_t)*8-1-p)));
    }

    uint8_t modifyCharBit(uint8_t n, int p, bool b) {
        return ((n & ~(1 << (sizeof(uint8_t)*8-1-p))) | (b << (sizeof(uint8_t)*8-1-p)));
    }

    bool getBit(block_t b, int i) {
        return (b << i) & (int) pow(2, (sizeof(block_t)*8-1));
    }

    bool getCharBit(uint8_t b, int i) {
        return (b << i) & (int) pow(2, (sizeof(uint8_t)*8-1));
    }

    block_t toggleBit(block_t b, int i) {
        return b ^ (1 << i);
    }

    int inList(uint8_t **list, uint8_t *testString, int listLen) {
        for (int i = 0; i < listLen; i++) {
            if (strcmp(list[i], testString) == 0) {
                return i;
            }
        }
        return 0;
    }
};

} // ns