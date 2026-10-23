/*
 * WxManager.cpp - open-meteo 氣象與空氣品質
 *
 * Endpoint（兩個，同一個 host，免金鑰）：
 *   /v1/forecast      → current_weather + daily temperature high/low
 *   /v1/air-quality   → current us_aqi
 *
 * open-meteo 免 API key、免費、rate limit 10k req/day（個人用途綽綽有餘）。
 * 回應極小（~500 bytes），ArduinoJson 解析壓力低。
 */

#include "WxManager.h"
#include "config.h"

// ── WMO weather code → 簡短英文文字（全 ASCII，配 LOAD_FONT2）──
String WxManager::wxText() const {
    switch (wxCode) {
        case  0: return F("Clear");
        case  1: return F("Mostly clear");
        case  2: return F("Partly cloudy");
        case  3: return F("Overcast");
        case  45:
        case 48: return F("Fog");
        case 51: case 53: case 55: return F("Light drizzle");
        case 56: case 57: return F("Freezing drz.");
        case 61: case 63: case 65: return F("Rain");
        case 66: case 67: return F("Heavy rain");
        case 71: case 73: case 75: return F("Snow");
        case 77: return F("Snow grains");
        case 80: case 81: case 82: return F("Showers");
        case 85: case 86: return F("Snow showers");
        case 95: return F("Thunderstorm");
        case 96: case 99: return F("Storm, hail");
        default:  return F("N/A");
    }
}

// ── WMO 天氣碼 → 圖標類型（7 種，全 ASCII 標籤）──
int WxManager::wxIconType(int code) {
    switch (code) {
        case 0: case 1:  return ICON_SUN;     // 晴 / 多半晴
        case 2:          return ICON_CLOUD;   // 多雲
        case 3:          return ICON_OVR;     // 陰
        case 45: case 48:return ICON_FOG;     // 霧
        case 51: case 53: case 55:
        case 56: case 57:
        case 61: case 63: case 65:
        case 66: case 67:
        case 80: case 81: case 82: return ICON_RAIN;  // 雨 / 毛毛雨
        case 95: case 96: case 99: return ICON_THST;   // 雷雨
        case 71: case 73: case 75: case 77:
        case 85: case 86:          return ICON_SNOW;    // 雪
        default:         return ICON_NONE;
    }
}

bool WxManager::fetchAll() {
    lastFetchMs = millis();

    // ── 1. 天氣預報（改用 HTTP，避免 WiFiClientSecure 崩潰）──
    {
        HTTPClient http;
        String url = String("http://api.open-meteo.com/v1/forecast?latitude=")
                   + String(LAT, 4) + String("&longitude=")
                   + String(LON, 4) +
                   "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
                   "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1"
                   "&timezone=Asia%2FTaipei";

        http.begin(url);
        http.setTimeout(15000);
        int code = http.GET();
        String raw = (code == 200) ? http.getString() : "";
        http.end();

        if (raw.length() == 0) {
            Serial.printf("[WX] forecast fetch HTTP %d\n", code);
            return false;
        }

        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            Serial.printf("[WX] forecast JSON parse: %s\n", err.c_str());
            return false;
        }

        JsonVariant cur = doc["current"];
        if (cur.isNull()) {
            Serial.println("[WX] forecast: current missing");
            return false;
        }

        tempNow = (int)cur["temperature_2m"].as<float>();
        humPct  = (int)cur["relative_humidity_2m"].as<float>();
        windKmh = (int)cur["wind_speed_10m"].as<float>();
        wxCode  = (int)cur["weather_code"].as<int>();

        JsonArray hiArr = doc["daily"]["temperature_2m_max"];
        JsonArray loArr = doc["daily"]["temperature_2m_min"];
        if (hiArr.size() > 0) tempHigh = (int)hiArr[0].as<float>();
        if (loArr.size() > 0) tempLow  = (int)loArr[0].as<float>();

        valid = true;
        Serial.printf("[WX] %dC %d%% wind %d km/h [%d] H%d/L%d\n",
                      tempNow, humPct, windKmh, wxCode, tempHigh, tempLow);
    }

    // ── 2. 空氣品質（同樣改用 HTTP）──
    {
        HTTPClient http;
        String url = String("http://air-quality-api.open-meteo.com/v1/air-quality?latitude=")
                   + String(LAT, 4) + String("&longitude=")
                   + String(LON, 4) +
                   "&current=us_aqi&timezone=Asia%2FTaipei";

        http.begin(url);
        http.setTimeout(15000);
        int code = http.GET();
        String raw = (code == 200) ? http.getString() : "";
        http.end();

        if (raw.length() == 0) {
            Serial.printf("[WX] air-quality fetch HTTP %d\n", code);
            // AQI 失敗不影響天氣顯示
            return true;
        }

        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, raw);
        if (err) {
            Serial.printf("[WX] air-quality JSON parse: %s\n", err.c_str());
            return true;
        }

        JsonVariant cur = doc["current"];
        if (!cur.isNull()) {
            int aqiVal = (int)cur["us_aqi"].as<int>();
            this->aqi = aqiVal;
            Serial.printf("[WX] AQI=%d\n", aqiVal);
        }
    }

    return true;
}
