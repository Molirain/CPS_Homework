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
    // 初始化舵机，添加引脚和180度舵机的最小(500us)、最大(2500us)脉宽参数
    blindServo.attach(PIN_SERVO, 500, 2500);
    
    // 改用 通道4 给 LED 使用，避开 ESP32Servo 底层默认占用的通道(0~3)
    ledcSetup(4, 5000, 8);
    ledcAttachPin(PIN_LED, 4);
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
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        bool stateChanged = false;
        if (sysState.blind_angle != localState.blind_angle || 
            sysState.light_level != localState.light_level) {
            sysState.blind_angle = localState.blind_angle;
            sysState.light_level = localState.light_level;
            stateChanged = true;
        }
        
        if (stateChanged) {
            saveState(); // 仅当真实发生状态改变时才写 Flash，防止每20ms疯狂擦写闪存导致死机
            g_lastReportTime = xTaskGetTickCount() - pdMS_TO_TICKS(1000); // 强制极速上报新的外设动作
            Serial.printf("[Hardware] 自动模式触发控制，更新状态并立即上报\n");
        }
        xSemaphoreGive(stateMutex);
    } 
    
    // 总电源关闭：驱动硬件 + 同步更新全局状态，避免上报不一致
    if (localState.power == "off") {
        localState.light_level = 0;
        localState.blind_angle = 0;
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        sysState.light_level = 0;
        sysState.blind_angle = 0;
        xSemaphoreGive(stateMutex);
    }

    // 驱动执行器舵机 (缓动降速平滑处理)
    if (current_blind_angle == -1) {
        current_blind_angle = localState.blind_angle;
    }
    
    // 如果舵机被 detach 断开了，在需要运动时重新 attach
    if (current_blind_angle != localState.blind_angle && !blindServo.attached()) {
        blindServo.attach(PIN_SERVO, 500, 2500);
    }

    if (abs(current_blind_angle - localState.blind_angle) > 2) {
        if (current_blind_angle < localState.blind_angle) current_blind_angle += 3;
        else current_blind_angle -= 3;
    } else {
        current_blind_angle = localState.blind_angle;
    }

    // 仅在角度发生变化时下发 PWM，防止连续调用引发定时器微抖动
    static int last_written_angle = -1;
    if (last_written_angle != current_blind_angle) {
        blindServo.write(current_blind_angle);
        last_written_angle = current_blind_angle;
    } else {
        // 到达终点后，切断 PWM 使其完全放松，消除100%的抽搐
        if (blindServo.attached()) {
            blindServo.detach();
        }
    }
    
    // 驱动调光呼吸灯占空比 0~3 档映射到 0~255
    int duty = 0;
    if (localState.light_level == 1) {
        duty = 85;
    } else if (localState.light_level == 2) {
        duty = 170;
    } else if (localState.light_level == 3) {
        duty = 255;
    }
    ledcWrite(4, duty);
}

void task_hardware(void* p)
{
    Moli_Hardware hardware;
    hardware.init();

    for (;;) {
        hardware.process();
        vTaskDelay(pdMS_TO_TICKS(20)); // 修改循环刷新率到 20ms 以支持平滑转动
    }
}
