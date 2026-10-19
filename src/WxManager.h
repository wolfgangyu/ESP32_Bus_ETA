/*
 * WxManager.h - 氣象 + 空氣品質（open-meteo）
 *
 * 免費、免金鑰、REST JSON；回應 ~500 bytes。
 * 抓 current_weather + daily 一次，解析後提供顯示用欄位。
 */

#ifndef WX_MANAGER_H
#define WX_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class WxManager {
private:
    // 南港區新新公園附近
    static constexpr double LAT = 25.054;
    static constexpr double LON = 121.585;
    unsigned long lastFetchMs = 0;
    unsigned long refreshMs   = 10 * 60 * 1000;   // 10 min

    int  tempNow   = 0;
    int  humPct    = 0;
    int  windKmh   = 0;
    int  wxCode    = -1;
    int  tempHigh  = 0;
    int  tempLow   = 0;
    int  aqi       = -1;    // -1 = 尚未抓到
    bool valid     = false;

    String wxText() const;
    String fetchAndParse();

public:
    WxManager() {}

    // 回 true = 本次成功、有可顯示的資料
    bool fetchAll();

    int  getTemp()   const { return tempNow; }
    int  getHum()    const { return humPct; }
    int  getWind()   const { return windKmh; }
    int  getWxHigh() const { return tempHigh; }
    int  getWxLow()  const { return tempLow; }
    int  getAqi()    const { return aqi; }
    String getWxText() const { return wxText(); }
    int  getWxCode() const { return wxCode; }
    int  getWxIconType() const { return wxIconType(wxCode); }
    bool isValid()   const { return valid; }

public:
    // 天氣圖標類型（供 UIManager 現畫，7 種全 ASCII 標籤）
    enum WxIcon { ICON_NONE=-1, ICON_SUN, ICON_CLOUD, ICON_OVR, ICON_RAIN, ICON_THST, ICON_FOG, ICON_SNOW };
    static int wxIconType(int code);
};

#endif
