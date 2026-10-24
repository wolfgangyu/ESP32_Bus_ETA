/*
 * TDXClient.cpp - TDX API 客戶端實作
 *
 * TDX API 文件：https://tdx.transportdata.tw/api-service/swagger
 * 公車即時到站：/api/basic/v2/Bus/EstimatedTimeOfArrival/City/Taipei
 */

#include "TDXClient.h"
#include "config.h"

TDXClient::TDXClient() {}

TDXClient::~TDXClient() {}

void TDXClient::begin(const String& clientId, const String& clientSecret) {
    this->clientId = clientId;
    this->clientSecret = clientSecret;
    this->accessToken = "";
    this->tokenExpiry = 0;

    #ifdef DEBUG_SERIAL
    Serial.println("TDX Client 初始化完成");
    #endif
}

String TDXClient::buildAuthHeader() {
    // Base64 編碼 client_id:client_secret
    String auth = clientId + ":" + clientSecret;
    String encoded = base64::encode(auth);
    return "Basic " + encoded;
}

String TDXClient::getAccessToken() {
    // Token 還有效就直接回傳
    if (isTokenValid() && accessToken.length() > 0) {
        return accessToken;
    }

    #ifdef DEBUG_SERIAL
    Serial.println("申請新的 TDX Access Token...");
    #endif

    HTTPClient http;
    secureClient.setInsecure();

    http.begin(secureClient, "https://tdx.transportdata.tw/auth/realms/TDXConnect/protocol/openid-connect/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", buildAuthHeader());

    String postData = "grant_type=client_credentials";
    int httpCode = http.POST(postData);
    callCount++;
    if (httpCode == 200) {
        String response = http.getString();
        dataBytes += response.length();
        DynamicJsonDocument doc(response.length() + 256);
        DeserializationError error = deserializeJson(doc, response);

        if (!error) {
            accessToken = doc["access_token"].as<String>();
            int expiresIn = doc["expires_in"].as<int>();
            tokenExpiry = millis() + (expiresIn - 60) * 1000;

            #ifdef DEBUG_SERIAL
            Serial.println("Access Token 取得成功");
            #endif
        } else {
            #ifdef DEBUG_SERIAL
            Serial.printf("JSON 解析失敗: %s\n", error.c_str());
            #endif
        }
    } else {
        #ifdef DEBUG_SERIAL
        Serial.printf("Token 請求失敗 HTTP: %d\n", httpCode);
        Serial.println(http.getString());
        #endif
    }

    http.end();
    return accessToken;
}

String TDXClient::fetchBusETA(const String& stopName) {
    String token = getAccessToken();
    if (token.length() == 0) {
        #ifdef DEBUG_SERIAL
        Serial.println("無法取得 Access Token");
        #endif
        return "";
    }

    HTTPClient http;
    secureClient.setInsecure();

    String encodedStopName = urlEncode(stopName);
    String url = String("https://tdx.transportdata.tw/api/basic/v2/Bus/EstimatedTimeOfArrival/City/") + TARGET_CITY
                 + "?$format=JSON&$filter=contains(StopName/Zh_tw,'" + encodedStopName + "')&$top=50";

    #ifdef DEBUG_SERIAL
    Serial.printf("請求 URL: %s\n", url.c_str());
    #endif

    http.begin(secureClient, url);
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();
    callCount++;

    String response = "";
    if (httpCode == 200) {
        response = http.getString();
        dataBytes += response.length();
        #ifdef DEBUG_SERIAL
        Serial.printf("API 回應長度: %d bytes\n", response.length());
        #endif
    } else if (httpCode == 401) {
        tokenExpiry = 0;
        #ifdef DEBUG_SERIAL
        Serial.println("Token 過期 (401)，下次將重新申請");
        #endif
    } else {
        #ifdef DEBUG_SERIAL
        Serial.printf("API 請求失敗 HTTP: %d\n", httpCode);
        Serial.println(http.getString());
        #endif
    }

    http.end();
    return response;
}

// 以站名一次抓回所有同名牌站牌的 ETA（新新公園共 22 支 StopUID，每支只回報各自路線）
// filter=contains(StopName/Zh_tw,'<name>') 不需預先列 StopUID，對 TDX 端最省事
String TDXClient::fetchBusETAByName(const String& stopNameZh) {
    String token = getAccessToken();
    if (token.length() == 0) return "";

    HTTPClient http;
    secureClient.setInsecure();

    String url = String("https://tdx.transportdata.tw/api/basic/v2/Bus/EstimatedTimeOfArrival/City/") + TARGET_CITY
                 + "?%24format=JSON&%24filter=contains(StopName/Zh_tw,'" + urlEncode(stopNameZh) + "')&%24top=100";

    #ifdef DEBUG_SERIAL
    Serial.printf("ETA URL: %s\n", url.c_str());
    #endif

    http.begin(secureClient, url);
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();
    callCount++;
    String response = "";

    if (httpCode == 200) {
        response = http.getString();
        dataBytes += response.length();
        #ifdef DEBUG_SERIAL
        Serial.printf("ETA 回應長度: %d bytes\n", response.length());
        #endif
    } else if (httpCode == 401) {
        tokenExpiry = 0;
    } else {
        #ifdef DEBUG_SERIAL
        Serial.printf("API 請求失敗 HTTP: %d\n", httpCode);
        String err = http.getString();
        if (err.length() > 0) Serial.printf("錯誤訊息: %s\n", err.substring(0, 300).c_str());
        #endif
    }

    http.end();
    return response;
}

// UTF-8 byte 安全的 URL encode（中文站名每 byte 獨立 encode，不拆 UTF-16）
String TDXClient::urlEncode(const String& str) {
    String encoded = "";
    const char* p = str.c_str();
    int len = str.length();
    char hex[4];
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += (char)c;
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}