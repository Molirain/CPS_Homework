#include "network_task.h"
#include <time.h>

void Moli_Network::setupNtp()
{
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    struct tm timeinfo;
    
    for (;;) {
        if (getLocalTime(&timeinfo)) {
            break;
        }
        Serial.println("Failed to obtain time, retrying...");
        delay(1000);
    }
    Serial.println("Time synchronized.");
}

void Moli_Network::init()
{
    // 初始化 Wi-Fi 与 SSL/TLS 的通信握手
    myMqtt.begin();
    
    // 网络连通后获得授时服务
    setupNtp();
}

void Moli_Network::process()
{
    TickType_t lastReportTime = xTaskGetTickCount();
    
    for (;;) {
        // 维持 MQTT 生命周期及重连
        myMqtt.loop();
        
        // 1 Hz 定时上传频率推送
        if (xTaskGetTickCount() - lastReportTime >= pdMS_TO_TICKS(1000)) {
            myMqtt.publishState();
            lastReportTime = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task_network(void* p)
{
    Moli_Network network;
    network.init();
    
    // 在这里开启阻塞循环服务
    network.process();
}
