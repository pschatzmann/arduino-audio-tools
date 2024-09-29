#include "AudioTools.h"
#include "Concurrency/QueueLockFree.h"

// select the queue implementation:
Queue<int> q1;
QueueFromVector<int> q2{10, 0};
QueueLockFree<int> q3(10);

template <typename T>
void test(T &q, const char* cls){
    Serial.println(cls);
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
    Serial.println("-> ok");
}

void setup() {
    Serial.begin(115200);
    test<Queue<int>>(q1,"Queue");
    test<QueueFromVector<int>>(q2,"QueueFromVector");
    test<QueueLockFree<int>>(q3,"QueueLockFree");
    Serial.println("all tests passed");
}

void loop(){}