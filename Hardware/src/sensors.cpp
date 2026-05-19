#include "sensors.h"
#include "config.h"
#include "mqtt.h"

Moli_Sensors::Moli_Sensors()
{
    lastBtnA = HIGH;
    lastBtnB = HIGH;
}

void Moli_Sensors::init()
{
    pinMode(PIN_LIGHT_ADC, INPUT);
    pinMode(PIN_HUMAN, INPUT);
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
}

void Moli_Sensors::process()
{
    int raw_lux = analogRead(PIN_LIGHT_ADC);
    
    // 使用 ADC 值按比例模拟 Lux 光照度
    sysState.lux = raw_lux * (1000.0 / 4095.0); 
    sysState.human = (digitalRead(PIN_HUMAN) == HIGH);

    bool btnA = digitalRead(PIN_BTN_A);
    bool btnB = digitalRead(PIN_BTN_B);

    // 按键 A 切换总电源
    if (btnA == LOW && lastBtnA == HIGH) {
        if (sysState.power == "on") {
            sysState.power = "off";
        } else {
            sysState.power = "on";
        }
        saveState(); // 持久化
    }
    
    // 按键 B 切换自动 / 手动模式
    if (btnB == LOW && lastBtnB == HIGH) {
        if (sysState.mode == "auto") {
            sysState.mode = "manual";
        } else {
            sysState.mode = "auto";
        }
        saveState(); // 持久化
    }
    
    lastBtnA = btnA;
    lastBtnB = btnB;

    // 将最新的输入端结果同步到系统核心队列中
    xQueueOverwrite(stateQueue, &sysState);
}

void task_sensors(void* p)
{
    Moli_Sensors sensors;
    sensors.init();

    for (;;) {
        sensors.process();
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms
    }
}
