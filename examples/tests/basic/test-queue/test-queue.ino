#include "AudioTools.h"
#include "Concurrency/QueueLockFree.h"

// select the queue implementation:
//Queue<int> q;
//QueueFromVector<int> q{10, 0};
QueueLockFree<int> q(10);

void setup(){
    Serial.begin(115200);
    assert(q.empty());

    for (int j=0;j<10;j++){
        q.enqueue(j);
        assert(!q.empty());
        assert(q.size()==j+1);
    }
    int number;
    for (int j=0;j<10;j++){
        q.dequeue(number);
        assert(number == j);
        assert(q.size()==10-(j+1));
    }
    assert(q.empty());
}

void loop(){}