/*
 * WiFiManager.cpp - WiFi 連線管理實作
 */

#include "WiFiManager.h"
#include "config.h"

WiFiManager::WiFiManager() {}

WiFiManager::~WiFiManager() {
    // 清除事件回調 - 不需要處理，用輪詢模式
}

void WiFiManager::onWiFiEvent(arduino_event_id_t event) {
    #ifdef DEBUG_SERIAL
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi Station 啟動");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFi 已連接 AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("取得 IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("WiFi 斷線");
            break;
        default:
            break;
    }
    #endif

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            connected = true;
            retryCount = 0;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            connected = false;
            break;
        default:
            break;
    }
}

void WiFiManager::begin(const String& ssid, const String& password) {
    this->ssid = ssid;
    this->password = password;

    WiFi.mode(WIFI_STA);

    // 使用正確的回調簽名 - WiFiEventCb 是 void (*)(arduino_event_id_t)
    WiFi.onEvent([](arduino_event_id_t event) {
        // 這裡無法直接存取 this，改用靜態函數或全域實例
        // 簡單做法：改用輪詢模式，不依賴事件回調
    });

    #ifdef DEBUG_SERIAL
    Serial.printf("連線 WiFi: %s\n", ssid.c_str());
    #endif

    WiFi.begin(ssid.c_str(), password.c_str());
    lastRetry = millis();
}

void WiFiManager::loop() {
    // 簡單輪詢模式檢查連線狀態
    bool wasConnected = connected;
    connected = (WiFi.status() == WL_CONNECTED);

    if (connected && !wasConnected) {
        retryCount = 0;
        #ifdef DEBUG_SERIAL
        Serial.printf("WiFi 已連線，IP: %s\n", WiFi.localIP().toString().c_str());
        #endif
    } else if (!connected && wasConnected) {
        #ifdef DEBUG_SERIAL
        Serial.println("WiFi 斷線");
        #endif
    }

    // 檢查連線狀態，需要時重連
    if (!connected && (millis() - lastRetry > WIFI_RETRY_INTERVAL)) {
        if (retryCount < MAX_RETRY) {
            #ifdef DEBUG_SERIAL
            Serial.printf("WiFi 重連嘗試 %d/%d\n", retryCount + 1, MAX_RETRY);
            #endif
            WiFi.reconnect();
            retryCount++;
            lastRetry = millis();
        } else {
            #ifdef DEBUG_SERIAL
            Serial.println("WiFi 重連次數超過上限，等待手動重試");
            #endif
        }
    }
}

void WiFiManager::reconnect() {
    retryCount = 0;
    lastRetry = 0;
    connected = false;
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());
}