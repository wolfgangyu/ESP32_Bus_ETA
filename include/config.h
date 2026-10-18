/*
 * config.h - 專案設定檔
 * 請複製為 config_local.h 並填入實際數值
 * config_local.h 已在 .gitignore 中，不會被提交
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================
//  Wi-Fi 設定
// =============================================
#ifndef WIFI_SSID
#define WIFI_SSID        "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"
#endif

// =============================================
//  TDX API 設定 (台北市公車即時到站)
// 申請位置：https://tdx.transportdata.tw/
// =============================================
#ifndef TDX_CLIENT_ID
#define TDX_CLIENT_ID    "YOUR_TDX_CLIENT_ID"
#endif

#ifndef TDX_CLIENT_SECRET
#define TDX_CLIENT_SECRET "YOUR_TDX_CLIENT_SECRET"
#endif

// =============================================
//  目標站牌設定
// =============================================
// 南港路三段新新公園站
// 站牌 ID 需從 TDX API 查詢確認，常見為 "Nangang_SunSunPark" 或類似
#define TARGET_STOP_NAME   "新新公園"
#define TARGET_STOP_ID     ""  // 若知道具體站牌 ID 請填入，否則用名稱比對

// 顯示設定
#define MAX_BUS_DISPLAY    6      // 最多顯示幾筆公車資訊
#define REFRESH_INTERVAL   30000  // 重新整理間隔 (毫秒) - 30 秒
#define WIFI_RETRY_INTERVAL 10000 // WiFi 重連嘗試間隔

// =============================================
//  韌體版本
// =============================================
// dev = 開發中；出 release 時改成 v0.1.0 等
#ifndef FW_VERSION
#define FW_VERSION "v0.1.0"
#endif

// =============================================
//  顯示介面設定
// =============================================
#define SHOW_IP_ON_STARTUP   true   // 開機顯示 IP
#define SHOW_HELLO_WORLD     true   // 開機顯示 Hello World
#define STARTUP_DELAY_MS     2000   // 開機訊息顯示時間

// 時區設定 (台北 UTC+8)
#define TIMEZONE_OFFSET      8      // 小時
#define USE_NTP              true   // 使用 NTP 校時

// =============================================
//  除錯設定
// =============================================
#define DEBUG_SERIAL         true
#define DEBUG_TDX_RAW        false  // 印出原始 API 回應 (佔記憶體)

#endif