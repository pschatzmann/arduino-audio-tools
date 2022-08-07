#include "AudioTools.h"
#include "AudioBasic/Collections.h"

void testLoop() {
    Serial.println();
    Serial.println("testLoop");
    List<const char*> list({"a","b","c"});
    assert(list.size()==3);
    for (int j=0;j<list.size();j++){
        Serial.print(list[j]);
    }
    Serial.println();
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }
    Serial.println();
    for (auto it=list.rbegin();it!=list.rend();it--){
        Serial.print(*it);
    }
}

void testPushBack() {
    Serial.println();
    Serial.println("testPushBack");
    List<const char*> list;
    assert(list.size()==0);
    assert(list.empty());
    list.push_back("a");
    assert(list.size()==1);
    assert(!list.empty());

    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }

    list.push_back("b");
    assert(list.size()==2);
    assert(!list.empty());
    Serial.println();
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }

    list.push_back("c");
    assert(list.size()==3);
    assert(!list.empty());
    Serial.println();
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }
    Serial.println();
    for (auto it=list.rbegin();it!=list.rend();it--){
        Serial.print(*it);
    }
}

void testPushFront() {
    Serial.println();
    Serial.println("testPushFront");
    List<const char*> list;
    assert(list.size()==0);
    assert(list.empty());
    list.push_front("a");
    assert(list.size()==1);
    assert(!list.empty());
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }

    list.push_front("b");
    assert(list.size()==2);
    assert(!list.empty());
    Serial.println();
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }

    list.push_front("c");
    assert(list.size()==3);
    assert(!list.empty());
    Serial.println();
    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }
    Serial.println();
    for (auto it=list.rbegin();it!=list.rend();it--){
        Serial.print(*it);
    }
}

void testPopFront(){
    Serial.println();
    Serial.println("testPopFront");
    List<const char*> list({"a","b","c"});
    assert(list.size()==3);
    const char* var="";
    for (int j=0;j<3;j++){
        list.pop_front(var);
        Serial.println(var);
        assert(list.size()==2-j);
    }
}

void testPopBack(){
    Serial.println();
    Serial.println("testPopBack");
    List<const char*> list({"a","b","c"});
    assert(list.size()==3);
    const char* var="";
    for (int j=0;j<3;j++){
        list.pop_back(var);
        Serial.println(var);
        assert(list.size()==2-j);
    }
}

void testInsert(){
    Serial.println();
    Serial.println("testInsert");
    List<const char*> list({"1","2","3"});

    auto it=list.begin();  
    list.insert(it,"0");
    list.insert(it+3,"4");

    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }
    Serial.println();
    for (auto it=list.rbegin();it!=list.rend();it--){
        Serial.print(*it);
    }

    assert(list.size()==5);  
}

void testErase(){
    Serial.println();
    Serial.println("testErase");
    List<const char*> list({"1","2","3"});

    auto it = list.begin()+2;
    assert(list.erase(it));

    for (auto it=list.begin();it!=list.end();it++){
        Serial.print(*it);
    }
    assert(list.size()==2);  

    list.clear();
    assert(list.empty());  

}

void setup(){
    Serial.begin(115200);
    testLoop();
    testPushBack();
    testPushFront();
    testPopFront();
    testPopBack();
    testInsert();
    testErase();
}

void loop(){

}