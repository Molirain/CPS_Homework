#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>

class Moli_Hardware {
private:
    Servo blindServo;
    bool isDaytime();

public:
    void init();
    void process();
};

// 暴露给 FreeRTOS 的任务指针
void task_hardware(void* p);
