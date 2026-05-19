#include <Arduino.h>
#include <Preferences.h>
#include "mqtt.h"
#include "sensors.h"
#include "hardware.h"
#include "network_task.h"

// 实例化全局数据与队列
SystemState sysState;
QueueHandle_t stateQueue;

// 从 NVS 加载上次断电前的状态，首次上电返回 false
static bool loadState()
{
    Preferences prefs;
    prefs.begin("moli", true);
    
    // 靠标志位判断是否初次启动
    if (!prefs.getBool("inited", false)) {
        prefs.end();
        return false;
    }
    
    sysState.mode = prefs.getString("mode", "auto");
    sysState.power = prefs.getString("power", "on");
    sysState.blind_angle = prefs.getInt("blind", 90);
    sysState.light_level = prefs.getInt("light", 0);
    
    prefs.end();
    return true;
}

// 将当前状态写入 NVS 持久化
void saveState()
{
    Preferences prefs;
    prefs.begin("moli", false);
    
    prefs.putBool("inited", true);
    prefs.putString("mode", sysState.mode);
    prefs.putString("power", sysState.power);
    prefs.putInt("blind", sysState.blind_angle);
    prefs.putInt("light", sysState.light_level);
    
    prefs.end();
}

void setup()
{
    Serial.begin(115200);

    // 尝试恢复断电前状态，若是第一次启动则维持默认值
    if (!loadState()) {
        Serial.println("[NVS] 首次启动，使用默认状态");
    } else {
        Serial.printf("[NVS] 恢复状态 mode=%s power=%s blind=%d light=%d\n",
                      sysState.mode.c_str(), sysState.power.c_str(),
                      sysState.blind_angle, sysState.light_level);
    }

    // 全局唯一状态队列，仅维持 1 个长度即可避免延迟堆积
    stateQueue = xQueueCreate(1, sizeof(SystemState));
    xQueueOverwrite(stateQueue, &sysState);

    // 各协程按实时性要求分级优先级
    // 硬件执行器 3 > 网络通信 2 > 传感器采集 1
    xTaskCreatePinnedToCore(task_network,  "NetworkTask",  10240, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_sensors,  "SensorsTask",  4096,  NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_hardware, "HardwareTask", 4096,  NULL, 3, NULL, 1);
}

void loop()
{
    vTaskDelay(portMAX_DELAY); // FreeRTOS 接管
}
