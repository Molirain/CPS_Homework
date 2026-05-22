#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

// 系统状态结构体
struct SystemState {
    String mode = "auto";
    String power = "on";
    int blind_angle = 90;
    int light_level = 0;
    float lux = 0.0;
    bool human = false;
};

extern SystemState sysState;
extern QueueHandle_t stateQueue;
extern SemaphoreHandle_t stateMutex;  // 保护 sysState 的互斥锁
extern TickType_t g_lastReportTime;   // 1Hz 上报计时器，onMessage 收到命令后重置
extern const char* root_ca;

// NVS 状态持久化（声明于 main.cpp 实现）
void saveState();

// Molirain 专属网络通信类
class Moli_MQTT {
private:
    WiFiClientSecure espClient;
    PubSubClient client;
    void setupWiFi();

public:
    Moli_MQTT();
    void begin();
    void connect();
    void loop();
    void publishState();
    void onMessage(char* topic, byte* payload, unsigned int length);
};
