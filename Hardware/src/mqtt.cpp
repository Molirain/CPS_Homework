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
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, msg);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }

        if (doc.containsKey("mode")) {
            sysState.mode = doc["mode"].as<String>();
        }
        if (doc.containsKey("power")) {
            sysState.power = doc["power"].as<String>();
        }
        if (doc.containsKey("blind_angle")) {
            sysState.blind_angle = doc["blind_angle"].as<int>();
        }
        if (doc.containsKey("light_level")) {
            sysState.light_level = doc["light_level"].as<int>();
        }
        
        // 全局队列覆盖上报
        xQueueOverwrite(stateQueue, &sysState);
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
    
    StaticJsonDocument<256> doc;
    doc["mode"] = sysState.mode;
    doc["power"] = sysState.power;
    doc["blind_angle"] = sysState.blind_angle;
    doc["light_level"] = sysState.light_level;
    doc["lux"] = sysState.lux;
    doc["human"] = sysState.human;
    
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("device/up", buffer);
}
