#include <Arduino.h>
#include "mqtt.h"
#include "sensors.h"
#include "hardware.h"
#include "network_task.h"

// 实例化全局数据与队列
SystemState sysState;
QueueHandle_t stateQueue;

void setup()
{
    Serial.begin(115200);

    // 全局唯一状态队列，仅维持 1 个长度即可避免延迟堆积
    stateQueue = xQueueCreate(1, sizeof(SystemState));
    xQueueOverwrite(stateQueue, &sysState);

    // 实例化各协程
    xTaskCreatePinnedToCore(task_network, "NetworkTask", 10240, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_sensors, "SensorsTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(task_hardware, "HardwareTask", 4096, NULL, 1, NULL, 1);
}

void loop()
{
    // C++ 传统的主 loop 退化为后台休眠
    vTaskDelay(portMAX_DELAY);
}
