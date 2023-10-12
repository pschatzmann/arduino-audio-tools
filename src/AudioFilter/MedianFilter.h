#pragma once
#include "Filter.h"

namespace audio_tools {


/**
 * @brief An embedded friendly, fast one-dimensional median filter algorithm implementation in C and C++
 * Useful for spike and noise removal from analog signals or other DSP
 * Also known as "salt-and-pepper noise" or "impulse noise" filter
 **/
template <typename T>
class MedianFilter : public Filter<T> {
  public:
    MedianFilter(int size=7) {
        medianBuffer.resize(size);
        medianFilter.numNodes = size;
        medianFilter.medianBuffer = medianBuffer;
        init(&medianFilter);
    };

    virtual T process(T in) override {
        return insert(&medianFilter, in);;
    }

  protected:
    struct sMedianNode_t {
        T value;                      //sample value
        struct sMedianNode *nextAge;    //pointer to next oldest value
        struct sMedianNode *nextValue;  //pointer to next smallest value
        struct sMedianNode *prevValue;  //pointer to previous smallest value
    };

    struct sMedianFilter_t {
        unsigned int numNodes;          //median node buffer length
        sMedianNode_t *medianBuffer;    //median node buffer
        sMedianNode_t *ageHead;         //pointer to oldest value
        sMedianNode_t *valueHead;       //pointer to smallest value
        sMedianNode_t *medianHead;      //pointer to median value
    };

    sMedianFilter_t medianFilter;
    Vector<sMedianNode_t> medianBuffer;

    int init(sMedianFilter_t *medianFilter){
        if(medianFilter && medianFilter->medianBuffer &&
            (medianFilter->numNodes % 2) && (medianFilter->numNodes > 1)){
            //initialize buffer nodes
            for(unsigned int i = 0; i < medianFilter->numNodes; i++){
                medianFilter->medianBuffer[i].value = 0;
                medianFilter->medianBuffer[i].nextAge = &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
                medianFilter->medianBuffer[i].nextValue = &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
                medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes].prevValue = &medianFilter->medianBuffer[i];
            }
            //initialize heads
            medianFilter->ageHead = medianFilter->medianBuffer;
            medianFilter->valueHead = medianFilter->medianBuffer;
            medianFilter->medianHead = &medianFilter->medianBuffer[medianFilter->numNodes / 2];

            return 0;
        }

        return -1;
    }

    int insert(sMedianFilter_t *medianFilter, T sample) {
        unsigned int i;
        sMedianNode_t *newNode, *it;

        if(medianFilter->ageHead == medianFilter->valueHead){
             //if oldest node is also the smallest node, increment value head
            medianFilter->valueHead = medianFilter->valueHead->nextValue;
        }

        if((medianFilter->ageHead == medianFilter->medianHead) ||
            (medianFilter->ageHead->value > medianFilter->medianHead->value)){
           //prepare for median correction
            medianFilter->medianHead = medianFilter->medianHead->prevValue;
        }

        //replace age head with new sample
        newNode = medianFilter->ageHead;
        newNode->value = sample;

        //remove age head from list
        medianFilter->ageHead->nextValue->prevValue = medianFilter->ageHead->prevValue;
        medianFilter->ageHead->prevValue->nextValue = medianFilter->ageHead->nextValue;
        //increment age head
        medianFilter->ageHead = medianFilter->ageHead->nextAge;

        //find new node position
        it = medianFilter->valueHead; //set iterator as value head
        for(i = 0; i < medianFilter->numNodes - 1; i++){
            if(sample < it->value){
                if(i == 0){   //replace value head if new node is the smallest
                    medianFilter->valueHead = newNode;
                }
                break;
            }
            it = it->nextValue;
        }

        //insert new node in list
        it->prevValue->nextValue = newNode;
        newNode->prevValue = it->prevValue;
        it->prevValue = newNode;
        newNode->nextValue = it;

        //adjust median node
        if(i >= (medianFilter->numNodes / 2)){
            medianFilter->medianHead = medianFilter->medianHead->nextValue;
        }

        return medianFilter->medianHead->value;
    }

};


}