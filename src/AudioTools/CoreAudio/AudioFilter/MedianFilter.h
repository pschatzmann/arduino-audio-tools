#pragma once
#include "Filter.h"
#include "AudioTools/CoreAudio/AudioBasic/Collections/Vector.h"

namespace audio_tools {

/**
 * @brief An embedded friendly, fast one-dimensional median filter algorithm
 *implementation in C and C++ Useful for spike and noise removal from analog
 *signals or other DSP Also known as "salt-and-pepper noise" or "impulse noise"
 *filter
 * @ingroup filter
 **/
template <typename T>
class MedianFilter : public Filter<T> {
 public:
  MedianFilter(int size = 7) {
    medianBuffer.resize(size);
    medianFilter.numNodes = size;
    medianFilter.medianBuffer = medianBuffer.data();
    init(&medianFilter);
  };

  virtual T process(T in) override { return insert(&medianFilter, in); }

 protected:
  struct MedianNode_t {
    T value = 0;                               // sample value
    struct MedianNode_t *nextAge = nullptr;    // pointer to next oldest value
    struct MedianNode_t *nextValue = nullptr;  // pointer to next smallest value
    struct MedianNode_t *prevValue =
        nullptr;  // pointer to previous smallest value
  };

  struct MedianFilter_t {
    unsigned int numNodes = 0;             // median node buffer length
    MedianNode_t *medianBuffer = nullptr;  // median node buffer
    MedianNode_t *ageHead = nullptr;       // pointer to oldest value
    MedianNode_t *valueHead = nullptr;     // pointer to smallest value
    MedianNode_t *medianHead = nullptr;    // pointer to median value
  };

  MedianFilter_t medianFilter;
  Vector<MedianNode_t> medianBuffer{0};

  int init(MedianFilter_t *medianFilter) {
    if (medianFilter && medianFilter->medianBuffer &&
        (medianFilter->numNodes % 2) && (medianFilter->numNodes > 1)) {
      // initialize buffer nodes
      for (unsigned int i = 0; i < medianFilter->numNodes; i++) {
        medianFilter->medianBuffer[i].value = 0;
        medianFilter->medianBuffer[i].nextAge =
            &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
        medianFilter->medianBuffer[i].nextValue =
            &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
        medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes].prevValue =
            &medianFilter->medianBuffer[i];
      }
      // initialize heads
      medianFilter->ageHead = medianFilter->medianBuffer;
      medianFilter->valueHead = medianFilter->medianBuffer;
      medianFilter->medianHead =
          &medianFilter->medianBuffer[medianFilter->numNodes / 2];

      return 0;
    }

    return -1;
  }

  int insert(MedianFilter_t *medianFilter, T sample) {
    unsigned int i;
    MedianNode_t *newNode=nullptr, *it=nullptr;

    if (medianFilter->ageHead == medianFilter->valueHead) {
      // if oldest node is also the smallest node, increment value head
      medianFilter->valueHead = medianFilter->valueHead->nextValue;
    }

    if ((medianFilter->ageHead == medianFilter->medianHead) ||
        (medianFilter->ageHead->value > medianFilter->medianHead->value)) {
      // prepare for median correction
      medianFilter->medianHead = medianFilter->medianHead->prevValue;
    }

    // replace age head with new sample
    newNode = medianFilter->ageHead;
    newNode->value = sample;

    // remove age head from list
    medianFilter->ageHead->nextValue->prevValue =
        medianFilter->ageHead->prevValue;
    medianFilter->ageHead->prevValue->nextValue =
        medianFilter->ageHead->nextValue;
    // increment age head
    medianFilter->ageHead = medianFilter->ageHead->nextAge;

    // find new node position
    it = medianFilter->valueHead;  // set iterator as value head
    for (i = 0; i < medianFilter->numNodes - 1; i++) {
      if (sample < it->value) {
        if (i == 0) {  // replace value head if new node is the smallest
          medianFilter->valueHead = newNode;
        }
        break;
      }
      it = it->nextValue;
    }

    // insert new node in list
    it->prevValue->nextValue = newNode;
    newNode->prevValue = it->prevValue;
    it->prevValue = newNode;
    newNode->nextValue = it;

    // adjust median node
    if (i >= (medianFilter->numNodes / 2)) {
      medianFilter->medianHead = medianFilter->medianHead->nextValue;
    }

    return medianFilter->medianHead->value;
  }
};

}  // namespace audio_tools