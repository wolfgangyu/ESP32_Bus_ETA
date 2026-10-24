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

#define TARGET_STOP_NAME   "新新公園"
#define TARGET_STOP_ID     ""  // 若知道具體站牌 ID 請填入，否則用名稱比對

// 顯示設定
// 注意：REFRESH_INTERVAL 已移除，主循環使用 REFRESH_INTERVAL_S (60s)
// MAX_BUS_DISPLAY 已移除，UIManager 使用 ITEMS_PER_PAGE (4)
#define WIFI_RETRY_INTERVAL 10000 // WiFi 重連嘗試間隔

// 觸控校準預設值 (XPT2046, 依板子實測調整)
#ifndef TOUCH_CAL_X0
#define TOUCH_CAL_X0  300
#endif
#ifndef TOUCH_CAL_Y0
#define TOUCH_CAL_Y0  300
#endif
#ifndef TOUCH_CAL_X1
#define TOUCH_CAL_X1  3800
#endif
#ifndef TOUCH_CAL_Y1
#define TOUCH_CAL_Y1  3800
#endif

// =============================================
//  韌體版本
// =============================================
// dev = 開發中；出 release 時改成 vX.Y.Z 等
#ifndef FW_VERSION
#define FW_VERSION "v1.1.0"
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

// LED PWM channel 分配 (供 LEDManager、LightSensor 共用)
// RGB LED 為共陽極 (Active Low)：GPIO4=紅, GPIO16=綠, GPIO17=藍
// 背光 GPIO27 為共陰極 (正向)
#define LED_CH_RED   0
#define LED_CH_GREEN 1
#define LED_CH_BLUE  2
#define LED_CH_BL    3

// 光感電阻 (LDR) 自動背光參數
#define LDR_PIN            34
#define LDR_ADC_ATTEN      ADC_11db
#define LDR_REFRESH_MS     30000UL   // 30 秒更新一次
#define LDR_RAW_MIN        50        // 暗環境 ADC 下限
#define LDR_RAW_MAX        3000      // 亮環境 ADC 上限
#define LDR_DUTY_MIN       30        // 最暗背光 duty (0-255)
#define LDR_DUTY_MAX       255       // 最亮背光 duty
#define LDR_DEADZONE       5         // 死區，避免抖動
#define LDR_SMOOTH_N       8         // 滑動平均樣本數

// =============================================
//  除錯設定
// =============================================
#define DEBUG_SERIAL         true
#define DEBUG_TDX_RAW        false  // 印出原始 API 回應 (佔記憶體)

#endif