/*
 * BusData.cpp - 公車資料結構與解析實作
 *
 * TDX v2 ETA API 實際回傳格式 (2026-10-11 實測):
 * [
 *   {
 *     "StopUID": "TPE12726",
 *     "StopName": {"Zh_tw": "西新里(新新公園)", "En": "Xixin Village (Xinxin Park)"},
 *     "RouteUID": "TPE15314",
 *     "RouteID": "15314",
 *     "RouteName": {"Zh_tw": "203", "En": "203"},
 *     "Direction": 0,
 *     "EstimateTime": 2262,        // 秒數（0=暫無資料）
 *     "StopStatus": 0,
 *     "SrcUpdateTime": "2026-10-11T11:46:15+08:00",
 *     "UpdateTime": "2026-10-11T11:46:20+08:00"
 *   }
 * ]
 *
 * 規則:
 * - EstimateTime 秒數是主要依據；0 = 暫無資料
 * - StopStatus 只作輔助標示
 * - 同一站牌同路線同方向：保留 EstimateTime 最小者（最近一班）
 * - 依 EstimateTime 升序排序（暫無資料排最後）
 */

#include "BusData.h"
#include "config.h"

BusData::BusData() {
    refresh();
}

void BusData::refresh() {
    for (int i = 0; i < MAX_BUSES; i++) {
        buses[i].routeName = "";
        buses[i].destination = "";
        buses[i].estimateTime = "";
        buses[i].timeStatus = "";
        buses[i].stopID = 0;
        buses[i].stopStatus = -1;
        buses[i].estimateSecs = -1;
        buses[i].isLowFloor = false;
    }
    count = 0;
    scrollIndex = 0;
}

// 秒數 → 顯示文字（全 ASCII，上屏用）
static void formatEstimateTime(int seconds, String& out) {
    if (seconds < 60) {
        out = "Now";
    } else {
        int minutes = (seconds + 30) / 60; // 四捨五入
        out = "~" + String(minutes) + "min";
    }
}

// StopStatus 數字轉輔助 ASCII 文字（僅上屏用）
static const char* stopStatusText(int status) {
    switch (status) {
        case 3: return "Arriving";
        case 4: return "At stop";
        default: return "";
    }
}

// 比對兩筆 BusInfo 是否同路線同方向同站牌
static bool isSameBus(const BusInfo& a, const BusInfo& b) {
    return a.routeName == b.routeName &&
           a.direction == b.direction &&
           a.stopID == b.stopID;
}

bool BusData::parseTDXResponse(const String& json) {
    return parseFromJson(json, true);
}

bool BusData::mergeTDXResponse(const String& json) {
    return parseFromJson(json, false);
}

bool BusData::parseFromJson(const String& json, bool clear) {
    #ifdef DEBUG_SERIAL
    Serial.printf("解析 JSON，長度: %d\n", json.length());
    #endif

    size_t capacity = json.length() + 256;
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        #ifdef DEBUG_SERIAL
        Serial.printf("JSON 解析失敗: %s\n", error.c_str());
        #endif
        return false;
    }

    if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        return parseFromAPI(arr, clear);
    }
    return false;
}

bool BusData::parseFromAPI(JsonArray& array, bool clear) {
    if (clear) refresh();

    for (JsonVariant item : array) {
        JsonObject obj = item.as<JsonObject>();
        BusInfo info;
        info.stopStatus = -1;
        info.estimateSecs = -1;
        info.stopID = 0;
        info.direction = 0;
        bool parsed = false;

        // 路線名稱（改存英文 En，螢幕全 ASCII；En 缺省時 fallback 到 Zh_tw）
        if (obj.containsKey("RouteName") && obj["RouteName"].is<JsonObject>()) {
            JsonObject rn = obj["RouteName"].as<JsonObject>();
            if (rn.containsKey("En")) {
                info.routeName = rn["En"].as<String>();
                parsed = true;
            } else if (rn.containsKey("Zh_tw")) {
                info.routeName = rn["Zh_tw"].as<String>();
                parsed = true;
            }
        }
        else if (obj.containsKey("RoutesID") || obj.containsKey("RouteID")) {
            info.routeName = obj["RoutesID"].isNull() ? obj["RouteID"].as<String>() : obj["RoutesID"].as<String>();
            parsed = true;
        }
        if (!parsed) continue;

        // 方向
        info.direction = obj.containsKey("Direction") ? obj["Direction"].as<int>() : 0;

        // 站牌名稱
        if (obj.containsKey("StopName") && obj["StopName"].is<JsonObject>()) {
            JsonObject sn = obj["StopName"].as<JsonObject>();
            if (sn.containsKey("Zh_tw")) {
                info.destination = sn["Zh_tw"].as<String>();
            }
        }
        else if (obj.containsKey("StopName")) {
            info.destination = obj["StopName"].as<String>();
        }

        // 站牌 ID
        if (obj.containsKey("StopID")) {
            info.stopID = obj["StopID"].as<int>();
        }

        // EstimateTime（秒數，主要依據）
        // EstimateTime=None（JSON null）或 0 代表 TDX 當下無 GPS 回報 → 直接跳過不顯示
        int estSecs = 0;
        if (obj.containsKey("EstimateTime") && !obj["EstimateTime"].isNull()) {
            estSecs = obj["EstimateTime"].as<int>();
        }
        if (estSecs <= 0) continue;   // 暫無資料不顯示

        info.estimateSecs = estSecs;
        formatEstimateTime(estSecs, info.estimateTime);

        // StopStatus（輔助）
        if (obj.containsKey("StopStatus")) {
            info.stopStatus = obj["StopStatus"].as<int>();
            info.timeStatus = stopStatusText(info.stopStatus);
        }

        // 低地板
        if (obj.containsKey("IsLowFloor") || obj.containsKey("LowFloor")) {
            String fs = obj["IsLowFloor"].isNull() ? obj["LowFloor"].as<String>() : obj["IsLowFloor"].as<String>();
            info.isLowFloor = (fs == "True" || fs == "1" || fs == "Y");
        }

        // 同路線同方向同站牌：保留 EstimateTime 最小者
        bool replaced = false;
        for (int i = 0; i < count; i++) {
            if (isSameBus(buses[i], info)) {
                // 新的更近（秒數較小）才覆蓋
                if (info.estimateSecs < buses[i].estimateSecs || info.estimateSecs == 0) {
                    buses[i] = info;
                }
                replaced = true;
                break;
            }
        }
        if (!replaced && count < MAX_BUSES) {
            buses[count++] = info;
        }
    }

    sortBuses();

    #ifdef DEBUG_SERIAL
    Serial.printf("解析完成，共 %d 筆資料\n", count);
    #endif

    return count > 0;
}

bool BusData::sortBuses() {
    // EstimateTime 升序；暫無資料（-1）排最後
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            int a = buses[j].estimateSecs < 0 ? 999999 : buses[j].estimateSecs;
            int b = buses[j+1].estimateSecs < 0 ? 999999 : buses[j+1].estimateSecs;
            if (a > b) {
                BusInfo temp = buses[j];
                buses[j] = buses[j+1];
                buses[j+1] = temp;
            }
        }
    }
    return true;
}

int BusData::getCount(int direction) const {
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (buses[i].direction == direction) n++;
    }
    return n;
}

const BusInfo* BusData::getBusAt(int direction, int index) const {
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (buses[i].direction == direction) {
            if (n == index) return &buses[i];
            n++;
        }
    }
    return nullptr;
}
