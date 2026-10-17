/*
 * BusData.h - 公車資料結構與解析
 */

#ifndef BUS_DATA_H
#define BUS_DATA_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct BusInfo {
    String routeName;     // 路線英文代號（TDX RouteName.En，如 "306Shuttle"；全 ASCII 上屏）
    String destination;   // 站牌全名，如 "西新里(新新公園)"
    String estimateTime;  // 顯示文字（"約21分後到" / "暫無資料"）
    String timeStatus;    // 輔助狀態文字（未知/行駛中/即將到站...）
    int  stopID;
    int  stopStatus;      // TDX StopStatus 數字（0=未知, 3=即將到站...）
    int  estimateSecs;    // EstimateTime 秒數（0=暫無資料），供排序與判斷
    int  direction;       // 0=西向(去程), 1=東向(回程)
    bool isLowFloor;      // 是否無障礙車輛
};

class BusData {
private:
    static const int MAX_BUSES = 20;
    BusInfo buses[MAX_BUSES];
    int count = 0;
    int visibleCount = 0;
    int scrollIndex = 0;
    String lastUpdateTime = "";

    bool sortBuses();
    bool parseToBusInfo(const BusInfo& src, BusInfo& dest);

public:
    BusData();

    // 解析第一支站牌 JSON（會清空現有資料）
    bool parseTDXResponse(const String& json);
    // 解析並合併第二支站牌 JSON（不清空，合併到現有資料）
    bool mergeTDXResponse(const String& json);
    bool parseFromJson(const String& json, bool clear);
    bool parseFromAPI(JsonArray& array, bool clear = true);

    // 依方向查詢：0=西向(去程), 1=東向(回程)
    int getCount(int direction) const;
    const BusInfo* getBusAt(int direction, int index) const;

    int getTotalCount() const { return count; }
    String getLastUpdateTime() const { return lastUpdateTime; }

    void refresh();
};

#endif