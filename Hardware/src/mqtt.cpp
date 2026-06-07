#include "mqtt.h"
#include <ArduinoJson.h>

// EMQX 根证书
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDRjCCAsugAwIBAgIQGp6v7G3o4ZtcGTFBto2Q3TAKBggqhkjOPQQDAzCBiDEL\n" \
"MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n" \
"eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n" \
"JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMjEwMzIy\n" \
"MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjBfMQswCQYDVQQGEwJHQjEYMBYGA1UEChMP\n" \
"U2VjdGlnbyBMaW1pdGVkMTYwNAYDVQQDEy1TZWN0aWdvIFB1YmxpYyBTZXJ2ZXIg\n" \
"QXV0aGVudGljYXRpb24gUm9vdCBFNDYwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAR2\n" \
"+pmpbiDt+dd34wc7qNs9Xzjoq1WmVk/WSOrsfy2qw7LFeeyZYX8QeccCWvkEN/U0\n" \
"NSt3zn8gj1KjAIns1aeibVvjS5KToID1AZTc8GgHHs3u/iVStSBDHBv+6xnOQ6Oj\n" \
"ggEgMIIBHDAfBgNVHSMEGDAWgBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAdBgNVHQ4E\n" \
"FgQU0SLaTFnxS18mOKqd1u7rDcP7qWEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB\n" \
"/wQFMAMBAf8wHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBEGA1UdIAQK\n" \
"MAgwBgYEVR0gADBQBgNVHR8ESTBHMEWgQ6BBhj9odHRwOi8vY3JsLnVzZXJ0cnVz\n" \
"dC5jb20vVVNFUlRydXN0RUNDQ2VydGlmaWNhdGlvbkF1dGhvcml0eS5jcmwwNQYI\n" \
"KwYBBQUHAQEEKTAnMCUGCCsGAQUFBzABhhlodHRwOi8vb2NzcC51c2VydHJ1c3Qu\n" \
"Y29tMAoGCCqGSM49BAMDA2kAMGYCMQCMCyBit99vX2ba6xEkDe+YO7vC0twjbkv9\n" \
"PKpqGGuZ61JZryjFsp+DFpEclCVy4noCMQCwvZDXD/m2Ko1HA5Bkmz7YQOFAiNDD\n" \
"49IWa2wdT7R3DtODaSXH/BiXv8fwB9su4tU=\n" \
"-----END CERTIFICATE-----\n";

Moli_MQTT::Moli_MQTT() : client(espClient)
{
}

void Moli_MQTT::setupWiFi()
{
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // 连接 Wi-Fi
    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            break;
        }
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // 强制指定DNS
    IPAddress dns1(8, 8, 8, 8);
    IPAddress dns2(114, 114, 114, 114);
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
}

void Moli_MQTT::begin()
{
    setupWiFi();

    // 配置根证书
    espClient.setCACert(root_ca);
    client.setBufferSize(1024);  // 网页下发 JSON 较大(~350B)，默认 256 不足以接收
    client.setServer(MQTT_SERVER, MQTT_PORT);

    // 设置回调函数
    client.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->onMessage(topic, payload, length);
    });
}

void Moli_MQTT::onMessage(char* topic, byte* payload, unsigned int length)
{
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.printf("[MQTT] topic: %s  payload: %s\n", topic, msg.c_str());

    // 解析云端下发的数据
    if (String(topic) == "device/down") {
        // 网页下发字段较多（20+），512 不够，用 1024
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, msg);
        if (error) {
            Serial.printf("deserializeJson() failed: %s  (msg len=%d)\n",
                          error.c_str(), msg.length());
            return;
        }

        // 加锁保护 sysState（跨核 String 操作非线程安全）
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        bool changed = false;
        if (doc.containsKey("mode")) {
            sysState.mode = doc["mode"].as<String>();
            changed = true;
        }
        if (doc.containsKey("power")) {
            sysState.power = doc["power"].as<String>();
            changed = true;
        }
        if (doc.containsKey("blind_angle")) {
            sysState.blind_angle = doc["blind_angle"].as<int>();
            changed = true;
        }
        if (doc.containsKey("light_level")) {
            sysState.light_level = doc["light_level"].as<int>();
            changed = true;
        }
        
        // 全局队列覆盖上报
        xQueueOverwrite(stateQueue, &sysState);

        // 持久化：网页端命令也写入 NVS，防止重启回退
        if (changed) {
            saveState();
        }
        xSemaphoreGive(stateMutex);

        // 立即上报新状态 + 重置 1Hz 定时器，防止定时器在命令处理前发出旧数据导致前端闪回
        publishState();
        g_lastReportTime = xTaskGetTickCount();

        Serial.printf("[MQTT] 命令已生效 mode=%s power=%s blind=%d light=%d\n",
                      sysState.mode.c_str(), sysState.power.c_str(),
                      sysState.blind_angle, sysState.light_level);
    } else if (String(topic) == "device/human") {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, msg);
        if (!error && doc.containsKey("human")) {
            xSemaphoreTake(stateMutex, portMAX_DELAY);
            sysState.human = doc["human"].as<bool>();
            xSemaphoreGive(stateMutex);
            // 这里可以不做上报，sensors.cpp 会检测 sysState.human 变化自动触发上报
        }
    }
}

void Moli_MQTT::connect()
{
    for (;;) {
        if (client.connected()) {
            break;
        }
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32_Node_" + String(random(0xffff), HEX);

        if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("Success!");
            client.subscribe("device/down");
            client.subscribe("device/human");
        } else {
            Serial.printf("Failed, rc=%d. Trying again in 5 seconds...\n", client.state());
            delay(5000);
        }
    }
}

void Moli_MQTT::loop()
{
    if (!client.connected()) {
        static unsigned long lastReconnect = 0;
        unsigned long now = millis();
        if (now - lastReconnect > 5000) {
            lastReconnect = now;
            connect();
        }
        return;
    }
    client.loop();
}

void Moli_MQTT::publishState()
{
    if (!client.connected()) {
        return;
    }
    
    // 加锁读取 sysState，防止跨核 String 读取时被并发写入破坏
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    StaticJsonDocument<1024> doc;
    doc["mode"] = sysState.mode;
    doc["power"] = sysState.power;
    doc["blind_angle"] = sysState.blind_angle;
    doc["light_level"] = sysState.light_level;
    doc["lux"] = sysState.lux;
    doc["human"] = sysState.human;
    xSemaphoreGive(stateMutex);
    
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("device/up", buffer);
}
