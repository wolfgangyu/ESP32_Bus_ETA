/*
 * TDXClient.h - TDX API 客戶端 (台北市公車即時到站)
 */

#ifndef TDX_CLIENT_H
#define TDX_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>

class TDXClient {
private:
    String clientId;
    String clientSecret;
    String accessToken;
    unsigned long tokenExpiry = 0;

    String getAccessToken();
    String buildAuthHeader();
    String urlEncode(const String& str);

public:
    TDXClient();
    ~TDXClient();

    void begin(const String& clientId, const String& clientSecret);
    String fetchBusETA(const String& stopName);
    String fetchBusETAByName(const String& stopNameZh);
    bool isTokenValid() const { return millis() < tokenExpiry; }

    // 系統資訊用：今日呼叫次數 + 累計資料量（KB）
    int  getCallCount() const { return callCount; }
    float getDataUsedKB() const { return (float)dataBytes / 1024.0f; }
    void resetCounters() { callCount = 0; dataBytes = 0; }

private:
    int  callCount = 0;     // 今日 TDX GET 次數（含 token）
    size_t dataBytes = 0;   // 累計回應 byte（含 token）
};

#endif