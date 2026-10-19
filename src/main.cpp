/*
 * main.cpp - HW-950 ESP32 公車到站提示（v2: 大字體、出門喵一眼）
 *
 * 螢幕：2.4" ILI9341，Bodmer/TFT_eSPI 驅動
 * 排線腳位：SCK=14 MOSI=13 MISO=12 CS=15 DC=2 BL=27（build_flags 注入）
 * 關鍵：init 前把觸控 CS=33、SD CS=5 拉高隔離，防 MISO 被扯低
 * NTP：time.stdtime.gov.tw（ROC 時區），開機 + 定時對時
 * LED：綠燈 GPIO17（共陰極：LOW=滅、HIGH=亮）；PWM 方波閃爍，連線中快閃、已連線慢閃、離線熄
 * 字型：只用 TFT_eSPI 內建 ASCII（LOAD_FONT2/4/6），畫面全英文，避免中文字型缺失
 * UI：Page 0 Dashboard / Page 1 Slideshow，觸控左右滑動切換
 */
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "config_local.h"
#include "WiFiManager.h"
#include "TDXClient.h"
#include "BusData.h"
#include "WxManager.h"
#include "TouchManager.h"
#include "LEDManager.h"
#include "SDManager.h"
#include "LightSensor.h"
#include "UIManager.h"
#include "Slideshow.h"
#include <time.h>

// 觸控 & SD 片選，init 前強制關閉隔離（防 MISO 匯流排被扯低）
#define TOUCH_CS 33
#define SD_CS    5

// 呼吸燈 + NTP
#define LED_GREEN  17   // 實測 17 點出綠光（共陰極）
#define NTP_GMT8_SEC 8*3600           // 台灣 GMT+8，無 DST
#define NTP_SERVER1  "time.nist.gov"
#define NTP_SERVER2  "time.stdtime.gov.tw"
#define NTP_REFRESH_MS 3600000UL   // 每小時重對一次時

TFT_eSPI    tft = TFT_eSPI();
WiFiManager wifi;
TDXClient   tdx;
BusData     busData;
WxManager   wx;
TouchManager touch;
LEDManager  led;
SDManager   sd;
LightSensor ldr;
UIManager   ui;
Slideshow   slideshow;

static const int REFRESH_INTERVAL_S = 60;
static const unsigned long WX_REFRESH_MS = 10 * 60 * 1000;  // 氣象/AQI 10 min
static unsigned long lastFetchMs = 0;
static unsigned long lastWxMs    = 0;
static unsigned long lastNtpMs = 0;

Page currentPage = PAGE_DASHBOARD;
bool pageSwitchPending = false;
Page pendingPage = PAGE_DASHBOARD;

// NTP：設定時鐘並同步
void syncTime() {
    // 固定 GMT+8（台灣無 DST），不依賴時區資料庫，避免 ROC 未被識別而掉回 UTC
    configTime(NTP_GMT8_SEC, 0, NTP_SERVER1, NTP_SERVER2);
    struct tm t;
    if (getLocalTime(&t, 5000)) {
        Serial.printf("[NTP] synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        lastNtpMs = millis();
    } else {
        Serial.println("[NTP] sync failed (will retry)");
    }
}

// 處理觸控事件
void handleTouch(TouchEvent ev) {
    if (ev.type == TOUCH_SWIPE_LEFT) {
        if (currentPage == PAGE_DASHBOARD) {
            pendingPage = PAGE_SLIDESHOW;
            pageSwitchPending = true;
        } else if (currentPage == PAGE_SLIDESHOW) {
            slideshow.prev();
            ui.setPhotoDirty();
        }
    }
    else if (ev.type == TOUCH_SWIPE_RIGHT) {
        if (currentPage == PAGE_SLIDESHOW) {
            pendingPage = PAGE_DASHBOARD;
            pageSwitchPending = true;
        }
    }
    else if (ev.type == TOUCH_TAP) {
        if (currentPage == PAGE_DASHBOARD) {
            if (ui.inInfoBtn(ev.x, ev.y)) {
                pendingPage = PAGE_SYSTEM;
                pageSwitchPending = true;
                Serial.println("[UI] (i) → SYSTEM");
            }
        }
        else if (currentPage == PAGE_SLIDESHOW) {
            if (ui.inInfoBtn(ev.x, ev.y)) {
                pendingPage = PAGE_SYSTEM;
                pageSwitchPending = true;
            }
            else if (ui.inPhoto(ev.x, ev.y)) {
                slideshow.togglePause();
                Serial.printf("[UI] slideshow paused=%d\n", slideshow.isPaused());
            }
        }
        else if (currentPage == PAGE_SYSTEM) {
            if (ui.inSystemClose(ev.x, ev.y)) {
                pendingPage = PAGE_DASHBOARD;
                pageSwitchPending = true;
                Serial.println("[UI] × → DASHBOARD");
            }
        }
    }
}

void doFetchAndShow() {
    // 一次抓回所有新新公園站牌（22 支 StopUID 各自回報各自路線）
    String all = tdx.fetchBusETAByName(TARGET_STOP_NAME);
    busData.parseTDXResponse(all);
    ui.setBusDirty();
    ui.setMarqueeDirty();
    // 公車資料更新，跑馬燈同步；主畫面公車列局部重繪
    Serial.printf("[TDX] fetched, total=%d, calls=%d, data=%.1fKB\n",
                  busData.getTotalCount(), tdx.getCallCount(), tdx.getDataUsedKB());
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== HW-950 Bus ETA v2 ===");
#ifdef FW_VERSION
    Serial.printf("[info] fw=%s build=%s\n", FW_VERSION, __DATE__);
#endif

    // strapping
    pinMode(12, OUTPUT); digitalWrite(12, LOW);

    // 【關鍵】隔離觸控 & SD CS，釋放 MISO
    pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
    pinMode(SD_CS,    OUTPUT); digitalWrite(SD_CS,    HIGH);
    delay(20);

    Serial.println("TFT_eSPI init...");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // 初始化觸控 (需在 tft.init() 之後)
    touch.begin(&tft);

    // 初始化 LED
    led.begin();

    // 開機 LED 自檢：紅→綠→藍各亮 500ms
    ledcWrite(0, 0);   // 紅亮
    ledcWrite(1, 255); ledcWrite(2, 255);
    delay(500);
    ledcWrite(0, 255); ledcWrite(1, 0); ledcWrite(2, 255);  // 綠亮
    delay(500);
    ledcWrite(0, 255); ledcWrite(1, 255); ledcWrite(2, 0);  // 藍亮
    delay(500);
    ledcWrite(0, 255); ledcWrite(1, 255); ledcWrite(2, 255); // 全滅
    delay(200);

    // 開機畫面：Repo 標題
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);  // FONT4
    tft.setCursor(20, 50);
    tft.println(F("ESP32 Bus ETA"));
    tft.setTextSize(1);
    tft.setTextColor(0x5555, TFT_BLACK);
    tft.setCursor(20, 85);
    tft.println(F("HW-950  ILI9341"));
    tft.setCursor(20, 100);
    tft.println(F("TDX + open-meteo"));

    // 連線中：快閃
    led.setState(LED_FAST);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 130);
    tft.println(F("Connecting WiFi..."));

    // Wi-Fi
    wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (!wifi.isConnected() && tries < 60) {
        wifi.loop();
        tries++;
        delay(2000);
        led.tick(millis());
        if (tries % 5 == 0) {
            tft.fillRect(20, 145, tft.width() - 40, 16, TFT_BLACK);
            tft.setCursor(20, 145);
            tft.printf("retry %d ...\n", tries / 5);
        }
    }

    if (wifi.isConnected()) {
        String ip = wifi.getIP();
        tft.fillRect(20, 130, tft.width() - 40, 40, TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(20, 130);
        tft.print(F("IP: "));
        tft.println(ip);
        Serial.printf("Connected, IP=%s\n", ip.c_str());
        delay(1200);

        // NTP 對時
        Serial.println("Syncing NTP...");
        syncTime();

        tdx.begin(TDX_CLIENT_ID, TDX_CLIENT_SECRET);

        // 開機就抓一次氣象/AQI（不等 10 min），讓底部條立刻有內容
        if (wx.fetchAll()) {
            Serial.printf("[WX] initial fetch OK, temp=%d aqi=%d\n", wx.getTemp(), wx.getAqi());
        } else {
            Serial.println("[WX] initial fetch failed, retry in 10 min");
        }
        lastWxMs = millis();

        // 初始化 SD 卡
        bool sdOk = sd.begin();
        if (sdOk) {
            Serial.println("[SD] Ready");
        } else {
            Serial.println("[SD] Not available (continue without SD)");
        }

        // 初始化光感電阻自動背光
        ldr.begin();

        // 初始化 UI Manager
        ui.begin(&tft, &busData, &wx, &wifi, &tdx, &sd, &ldr);
        ui.setSlideshowSource(&slideshow);

        // 初始化 Slideshow
        slideshow.begin(&tft);

        // 上電先抓一次公車
        doFetchAndShow();
        lastFetchMs = millis();
        led.setState(LED_HEARTBEAT);
    } else {
        tft.fillRect(20, 130, tft.width() - 40, 40, TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(20, 130);
        tft.println(F("WiFi failed"));
        Serial.println("[FATAL] Wi-Fi 連線失敗");
        led.setState(LED_RED_ON);
        return;
    }
}

void loop() {
    unsigned long now = millis();

    wifi.loop();
    led.tick(now);
    ldr.tick(now);

    // Touch handling
    TouchEvent ev = touch.read();
    if (ev.type != TOUCH_NONE) {
        handleTouch(ev);
    }

    // Page switch handling
    if (pageSwitchPending) {
        currentPage = pendingPage;
        pageSwitchPending = false;
        ui.requestFullRedraw();
        Serial.printf("[UI] Switch to page %d\n", currentPage);
    }

    // 斷線：紅燈常亮
    if (!wifi.isConnected()) {
        led.setState(LED_RED_ON);
    } else {
        led.setState(LED_HEARTBEAT);
    }

    // 定期 NTP 重對時
    if (wifi.isConnected() && now - lastNtpMs >= NTP_REFRESH_MS) {
        syncTime();
    }

    // 定期抓公車資料 (60s)
    if (wifi.isConnected() && now - lastFetchMs >= (unsigned long)REFRESH_INTERVAL_S * 1000) {
        lastFetchMs = now;
        doFetchAndShow();
    }

    // 氣象/AQI 10 min
    if (wifi.isConnected() && now - lastWxMs >= WX_REFRESH_MS) {
        lastWxMs = now;
        if (wx.fetchAll()) {
            ui.setWeatherDirty();
            ui.setMarqueeDirty();
        }
    }

    // Slideshow auto-advance
    slideshow.tick(now);

    // --- Rendering ---

    // 頁面切換全重繪
    if (ui.fullRedrawNeeded) {
        ui.drawPage(currentPage);
    }

    // 各頁局部更新
    if (currentPage == PAGE_DASHBOARD) {
        ui.updateClock();
        if (ui.busListDirty) ui.updateBusList();   // 含自動捲動
        if (ui.weatherDirty) ui.requestFullRedraw(); // 天氣條重繪需連圖標一起重畫
    }
    else if (currentPage == PAGE_SLIDESHOW) {
        ui.updateClock();
        ui.updateMarquee();
        if (ui.photoDirty) ui.updateSlideshowPhoto();
    }
    else if (currentPage == PAGE_SYSTEM) {
        // 系統頁無動態，進頁時已全繪；NTP 到點順帶更新日期
        ui.updateClock();
    }

    delay(50);
}