#pragma once
#include "mqtt.h"

class Moli_Network {
private:
    Moli_MQTT myMqtt;
    void setupNtp();

public:
    void init();
    void process();
};

// 暴露给 FreeRTOS 的任务指针
void task_network(void* p);
