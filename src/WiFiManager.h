/*
 * WiFiManager.h - WiFi 連線管理
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
private:
    String ssid;
    String password;
    bool connected = false;
    unsigned long lastRetry = 0;
    int retryCount = 0;
    static const int MAX_RETRY = 10;

    void onWiFiEvent(WiFiEvent_t event);

public:
    WiFiManager();
    ~WiFiManager();

    void begin(const String& ssid, const String& password);
    void loop();
    void reconnect();

    bool isConnected() const { return connected; }
    String getIP() const { return WiFi.localIP().toString(); }
    String getSSID() const { return WiFi.SSID(); }
    int getRSSI() const { return WiFi.RSSI(); }
    int getRetryCount() const { return retryCount; }
};

#endif