#pragma once
#include <Arduino.h>

class Moli_Sensors {
private:
    bool lastBtnA;
    bool lastBtnB;

public:
    Moli_Sensors();
    void init();
    void process();
};

// 暴露给 FreeRTOS 的任务指针
void task_sensors(void* p);
