#include "AudioTools.h"
#include "AudioTools/Concurrency/QueueLockFree.h"
#include "AudioTools/Concurrency/RTOS/QueueRTOS.h"

// test different queue implementations:

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
    Serial.println("ok");
}

void setup() {
    Serial.begin(115200);

    Queue<int> q1;
    test<Queue<int>>(q1,"Queue");

    QueueFromVector<int> q2{10, 0};
    test<QueueFromVector<int>>(q2,"QueueFromVector");
    
    QueueLockFree<int> q3(10);
    test<QueueLockFree<int>>(q3,"QueueLockFree");

    QueueRTOS<int> q4(10);
    test<QueueRTOS<int>>(q4,"QueueRTOS");

    Serial.println("all tests passed");
}

void loop(){}