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
};

#endif