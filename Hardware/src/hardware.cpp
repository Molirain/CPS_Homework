#include "hardware.h"
#include "config.h"
#include "mqtt.h"
#include <time.h>

bool Moli_Hardware::isDaytime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return true; // NTP 时间未就绪时默认白天
    }
    
    // 07:00 ~ 19:00 表示白天
    if (timeinfo.tm_hour >= 7 && timeinfo.tm_hour < 19) {
        return true;
    }
    return false;
}

void Moli_Hardware::init()
{
    blindServo.attach(PIN_SERVO);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PIN_LED, 0);
}

void Moli_Hardware::process()
{
    SystemState localState;
    // 获取队列中的最新控制状态，如果没有则阻塞
    xQueuePeek(stateQueue, &localState, portMAX_DELAY);

    // AI边缘侧算法：当设为 automatically 且电源为 on
    if (localState.mode == "auto" && localState.power == "on") {
        if (isDaytime()) {
            localState.blind_angle = 90;
            if (localState.human && localState.lux < 200) {
                localState.light_level = 2; // 白天光线由于某些原因不足且有人，中等补光
            } else {
                localState.light_level = 0;
            }
        } else {
            localState.blind_angle = 0; // 晚上关百叶窗
            if (localState.human) {
                localState.light_level = 3; // 晚上有人，全功率照明
            } else {
                localState.light_level = 0;
            }
        }
        // 更新系统全局状态对象 (给 Network 使用做 Upstream 凭据)
        sysState.blind_angle = localState.blind_angle;
        sysState.light_level = localState.light_level;
        saveState(); // 自动算法每轮调节后持久化
    } 
    
    // 总电源关闭
    if (localState.power == "off") {
        localState.light_level = 0;
        localState.blind_angle = 0;
    }

    // 驱动执行器舵机
    blindServo.write(localState.blind_angle);
    
    // 驱动调光呼吸灯占空比 0~3 档映射到 0~255
    int duty = 0;
    if (localState.light_level == 1) {
        duty = 85;
    } else if (localState.light_level == 2) {
        duty = 170;
    } else if (localState.light_level == 3) {
        duty = 255;
    }
    ledcWrite(0, duty);
}

void task_hardware(void* p)
{
    Moli_Hardware hardware;
    hardware.init();

    for (;;) {
        hardware.process();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
