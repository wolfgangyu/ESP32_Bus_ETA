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
 * 觸控：獨立 XPT2046 Driver (CS=33, IRQ=36)，與螢幕共用 SPI 總線
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

// Touch 初始化狀態
bool touchInitialized = false;
unsigned long touchInitTime = 0;

// Splash 畫面狀態
bool splashFinished = false;

// NTP：設定時鐘並同步
void syncTime() {
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

// 處理觸控事件（簡單的頁面切換邏輯）
void handleTouch(TouchEvent ev) {
    if (ev.type == TOUCH_TAP) {
        if (currentPage == PAGE_DASHBOARD) {
            // Page 0: 點擊底部區域 (y > 200) 切換到 Slideshow
            if (ev.y > 200) {
                pendingPage = PAGE_SLIDESHOW;
                pageSwitchPending = true;
                Serial.println("[UI] Tap bottom -> SLIDESHOW");
            }
        } else if (currentPage == PAGE_SLIDESHOW) {
            // Page 1: 任意點擊返回 Dashboard
            pendingPage = PAGE_DASHBOARD;
            pageSwitchPending = true;
            Serial.println("[UI] Tap -> DASHBOARD");
        }
    } else if (ev.type == TOUCH_SWIPE_LEFT) {
        if (currentPage == PAGE_DASHBOARD) {
            // Page 0: 左滑切換到 Slideshow
            pendingPage = PAGE_SLIDESHOW;
            pageSwitchPending = true;
            Serial.println("[UI] Swipe left -> SLIDESHOW");
        }
    } else if (ev.type == TOUCH_SWIPE_RIGHT) {
        if (currentPage == PAGE_SLIDESHOW) {
            // Page 1: 右滑返回 Dashboard
            pendingPage = PAGE_DASHBOARD;
            pageSwitchPending = true;
            Serial.println("[UI] Swipe right -> DASHBOARD");
        }
    } else if (ev.type == TOUCH_HOLD) {
        // 長按進入校準模式
        if (currentPage == PAGE_DASHBOARD && ev.y < 40) {  // 天氣列區域長按
            Serial.printf("[TOUCH] HOLD at (%d,%d) -> ENTER CAL\n", ev.x, ev.y);
        }
    }
}

void doFetchAndShow() {
    String all = tdx.fetchBusETAByName(TARGET_STOP_NAME);
    busData.parseTDXResponse(all);
    ui.setBusDirty();
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

    // strapping: GPIO12 必須拉低
    pinMode(12, OUTPUT); digitalWrite(12, LOW);

    // 【關鍵】隔離觸控 & SD CS，釋放 MISO
    pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
    pinMode(SD_CS,    OUTPUT); digitalWrite(SD_CS,    HIGH);
    delay(20);

    Serial.println("TFT_eSPI init...");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // 初始化 LED
    led.begin();

    // 開機 LED 自檢：紅→綠→藍各亮 500ms
    ledcWriteChannel(0, 0);   // 紅亮
    ledcWriteChannel(1, 255); ledcWriteChannel(2, 255);
    delay(500);
    ledcWriteChannel(0, 255); ledcWriteChannel(1, 0); ledcWriteChannel(2, 255);  // 綠亮
    delay(500);
    ledcWriteChannel(0, 255); ledcWriteChannel(1, 255); ledcWriteChannel(2, 0);  // 藍亮
    delay(500);
    ledcWriteChannel(0, 255); ledcWriteChannel(1, 255); ledcWriteChannel(2, 255); // 全滅
    delay(200);

    // 開機畫面
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

    // Wi-Fi 連線
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
        yield();  // 餵狗
    }

    if (!wifi.isConnected()) {
        tft.fillRect(20, 130, tft.width() - 40, 40, TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(20, 130);
        tft.println(F("WiFi failed"));
        Serial.println("[FATAL] Wi-Fi 連線失敗");
        led.setState(LED_RED_ON);
        return;
    }

    // WiFi 成功
    String ip = wifi.getIP();
    tft.fillRect(20, 130, tft.width() - 40, 40, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(20, 130);
    tft.print(F("IP: "));
    tft.println(ip);
    Serial.printf("Connected, IP=%s\n", ip.c_str());
    delay(1200);
    Serial.println("[DBG] After WiFi delay");

    // NTP 對時
    Serial.println("Syncing NTP...");
    syncTime();
    Serial.println("[DBG] After NTP");

    Serial.println("[DBG] Before TDX begin");
    tdx.begin(TDX_CLIENT_ID, TDX_CLIENT_SECRET);
    Serial.println("[DBG] After TDX begin");

    // 開機就抓一次氣象/AQI
    Serial.println("[DBG] Before wx.fetchAll");
    if (wx.fetchAll()) {
        Serial.printf("[WX] initial fetch OK, temp=%d aqi=%d\n", wx.getTemp(), wx.getAqi());
    } else {
        Serial.println("[WX] initial fetch failed, retry in 10 min");
    }
    Serial.println("[DBG] After wx.fetchAll");
    lastWxMs = millis();

    // 初始化 SD 卡
    Serial.println("[DBG] Before sd.begin");
    bool sdOk = sd.begin();
    if (sdOk) Serial.println("[SD] Ready");
    else Serial.println("[SD] Not available");
    Serial.println("[DBG] After sd.begin");

    // 初始化光感電阻自動背光
    Serial.println("[DBG] Before ldr.begin");
    ldr.begin();
    Serial.println("[DBG] After ldr.begin");

    // 初始化 UI Manager
    Serial.println("[DBG] Before ui.begin");
    ui.begin(&tft, &busData, &wx, &wifi, &tdx, &sd, &ldr);
    ui.setSlideshowSource(&slideshow);
    Serial.println("[DBG] After ui.begin");

    // 初始化 Slideshow
    Serial.println("[DBG] Before slideshow.begin");
    slideshow.begin(&tft);
    Serial.println("[DBG] After slideshow.begin");

    // 上電先抓一次公車
    Serial.println("[DBG] Before doFetchAndShow");
    doFetchAndShow();
    Serial.println("[DBG] After doFetchAndShow");
    lastFetchMs = millis();
    led.setState(LED_HEARTBEAT);

    Serial.println("[SETUP] Done, entering loop");
}

void loop() {
    unsigned long now = millis();

    wifi.loop();
    led.tick(now);
    ldr.tick(now);

    // Splash 畫面處理 (3秒後自動結束)
    if (!splashFinished && ui.isShowingSplash()) {
        if (now - ui.getSplashStartMs() >= 3000) {
            ui.setShowSplash(false);
            splashFinished = true;
            ui.requestFullRedraw();
        }
        // Splash 期間也要更新畫面
        if (ui.fullRedrawNeeded) {
            ui.drawPage(PAGE_DASHBOARD);
        }
    }

    // 延遲初始化觸控 (等系統穩定 3 秒後)
    if (!touchInitialized && now - touchInitTime >= 3000) {
        touch.begin();
        touchInitialized = true;
        Serial.println("[TOUCH] Direct SPI touch initialized");
    }

    // Touch handling (只在初始化後)
    if (touchInitialized) {
        TouchEvent ev = touch.read();
        if (ev.type != TOUCH_NONE) {
            Serial.printf("[TOUCH] ev.type=%d x=%d y=%d z=%d\n", ev.type, ev.x, ev.y, ev.z);
            handleTouch(ev);
        }
    }

    // Page switch handling
    if (pageSwitchPending) {
        currentPage = pendingPage;
        pageSwitchPending = false;
        ui.requestFullRedraw();
        Serial.printf("[UI] Switch to page %d\n", currentPage);
    }

    // LED 狀態
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
        }
    }

    // Slideshow auto-advance
    slideshow.tick(now);

    // --- Rendering ---
    // Splash 畫面優先
    if (!splashFinished && ui.isShowingSplash()) {
        // Splash 已在上面處理
    } else if (currentPage == PAGE_DASHBOARD) {
        ui.updateClock();
        if (ui.busListDirty) ui.updateBusList();
        if (ui.weatherDirty) ui.requestFullRedraw();
    } else if (currentPage == PAGE_SLIDESHOW) {
        ui.updateClock();
        if (ui.photoDirty) ui.updateSlideshowPhoto();
    } else if (currentPage == PAGE_CALIBRATION) {
        ui.updateClock();
    }

    if (ui.fullRedrawNeeded) {
        ui.drawPage(currentPage);
    }

    // 強制餵狗
    yield();
    delay(50);
}