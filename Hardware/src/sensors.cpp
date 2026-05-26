#include "sensors.h"
#include "config.h"
#include "mqtt.h"

Moli_Sensors::Moli_Sensors()
{
    // 改为了下拉模式，所以初始未按下时的状态应该是 LOW
    lastBtnA = LOW;
    lastBtnB = LOW;
    lastHumanLvl = LOW;
    lastReportedHuman = LOW;
}

void Moli_Sensors::init()
{
    pinMode(PIN_LIGHT_ADC, INPUT);
    // 给模拟红外人体的引脚加上内部下拉电阻，防止引脚悬空收集环境噪声引发无规律乱跳
    pinMode(PIN_HUMAN, INPUT_PULLDOWN); 
    pinMode(PIN_BTN_A, INPUT_PULLDOWN);
    pinMode(PIN_BTN_B, INPUT_PULLDOWN);
}

void Moli_Sensors::process()
{
    int raw_lux = analogRead(PIN_LIGHT_ADC);
    
    // 加锁保护 sysState（跨核 String 操作非线程安全）
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    
    // 使用 ADC 值按比例模拟 Lux 光照度
    sysState.lux = raw_lux * (1000.0 / 4095.0); 

    bool currentHuman = digitalRead(PIN_HUMAN);
    // 用按钮模拟人体模块（按一次开，再按一次关）
    if (currentHuman == HIGH && lastHumanLvl == LOW) {
        sysState.human = !sysState.human; // 翻转有人/无人状态
    }

    bool btnA = digitalRead(PIN_BTN_A);
    bool btnB = digitalRead(PIN_BTN_B);

    // 按键 A 切换总电源 (下拉模式：按下时为 HIGH)
    bool shouldTriggerReport = false;

    if (btnA == HIGH && lastBtnA == LOW) {
        if (sysState.power == "on") {
            sysState.power = "off";
        } else {
            sysState.power = "on";
        }
        Serial.printf("[Sensor] 按键 A 被按下: 电源状态切换为 %s\n", sysState.power.c_str());
        saveState(); // 持久化
        shouldTriggerReport = true;
    }
    
    // 按键 B 切换自动 / 手动模式 (下拉模式：按下时为 HIGH)
    if (btnB == HIGH && lastBtnB == LOW) {
        if (sysState.mode == "auto") {
            sysState.mode = "manual";
        } else {
            sysState.mode = "auto";
        }
        Serial.printf("[Sensor] 按键 B 被按下: 运行模式切换为 %s\n", sysState.mode.c_str());
        saveState(); // 持久化
        shouldTriggerReport = true;
    }

    // 监测人体传感器变化
    if (sysState.human != lastReportedHuman) {
        Serial.printf("[Sensor] 人体检测状态改变: %s\n", sysState.human ? "有人" : "无人");
        shouldTriggerReport = true;
        lastReportedHuman = sysState.human;
    }
    
    lastBtnA = btnA;
    lastBtnB = btnB;
    lastHumanLvl = currentHuman; // 更新物理按压历史

    // 将最新的输入端结果同步到系统核心队列中
    xQueueOverwrite(stateQueue, &sysState);
    xSemaphoreGive(stateMutex);

    // 强制触发 Network 任务立即进行 MQTT 上报
    if (shouldTriggerReport) {
        g_lastReportTime = xTaskGetTickCount() - pdMS_TO_TICKS(1000);
    }
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
