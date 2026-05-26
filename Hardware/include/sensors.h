#pragma once
#include <Arduino.h>

class Moli_Sensors {
private:
    bool lastBtnA;
    bool lastBtnB;
    bool lastHumanLvl;
    bool lastReportedHuman;

public:
    Moli_Sensors();
    void init();
    void process();
};

void task_sensors(void* p);
