/*
 * main.cpp - HW-950 ESP32 公車到站提示（正式整合）
 *
 * 螢幕：2.4" ILI9341，Bodmer/TFT_eSPI 驅動
 * 排線腳位：SCK=14 MOSI=13 MISO=12 CS=15 DC=2 BL=27（build_flags 注入）
 * 關鍵：init 前把觸控 CS=33、SD CS=5 拉高隔離，防 MISO 被扯低
 * NTP：time.stdtime.gov.tw（ROC 時區），開機 + 定時對時
  * LED：綠燈 GPIO17（共陰極：LOW=滅、HIGH=亮）；PWM 方波閃爍，連線中快閃、已連線慢閃、離線熄
 * 字型：只用 TFT_eSPI 內建 ASCII（LOAD_FONT2），畫面全英文，避免中文字型缺失
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

// 時鐘獨立局部重繪（選 A：與 60s 车次 redraw 脫鉤）
#define CLOCK_CHECK_MS   500UL     // 檢查週期
#define CLOCK_X          140       // 表頭 x（W=240，"10/17 20:29" = 11 字元 = 66px，140+66=206 < 240）
#define CLOCK_Y          3
#define CLOCK_W          70        // 11 字元 × 6px + 邊距
#define CLOCK_H          10

TFT_eSPI    tft = TFT_eSPI();
WiFiManager wifi;
TDXClient   tdx;
BusData     busData;
WxManager   wx;

static const int REFRESH_INTERVAL_S = 60;
static const unsigned long WX_REFRESH_MS = 10 * 60 * 1000;  // 氣象/AQI 10 min
static unsigned long lastFetchMs = 0;
static unsigned long lastWxMs    = 0;
static unsigned long lastNtpMs = 0;
static char lastClockStr[12] = "";      // 上次畫上的時鐘字串（"MM/DD HH:MM"=11字+null）
static unsigned long lastClockCheckMs = 0;

// ── 呼吸燈 ──
enum LedState { LED_OFF, LED_FAST, LED_SLOW };
static LedState ledState = LED_OFF;
static unsigned long lastLedTick = 0;
static unsigned int ledPhase = 0;

void ledBegin() {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(LED_GREEN, 0);
    pinMode(4,  OUTPUT); digitalWrite(4,  LOW);
    pinMode(16, OUTPUT); digitalWrite(16, LOW);
    // 共陰極：LOW=滅，HIGH=亮；PWM duty 越高越亮
}

void ledTick(unsigned long now) {
    static const unsigned long PERIOD_FAST = 500;   // 連線中：0.5s 快閃
    static const unsigned long PERIOD_SLOW = 1000;  // 已連線：1s 慢閃
    if (ledState == LED_OFF) { ledcWrite(0, 0); return; }
    if (now - lastLedTick < 20) return;
    lastLedTick = now;
    unsigned long period = (ledState == LED_FAST) ? PERIOD_FAST : PERIOD_SLOW;
    ledPhase = (ledPhase + (unsigned int)(period / 256)) & 0xFF;
    static const unsigned char cosTab[128] = {
        0,7,14,22,29,36,44,51,59,66,73,80,88,95,102,109,115,122,129,135,142,148,154,160,166,172,177,182,187,192,196,201,
        205,208,212,215,218,221,224,226,228,231,233,234,236,237,238,239,240,241,241,242,242,243,243,243,243,243,243,243,
        242,242,241,241,240,239,238,237,236,234,233,231,228,226,224,221,218,215,212,208,205,201,196,192,187,182,177,172,
        166,160,154,148,142,135,129,122,115,109,102,95,88,80,73,66,59,51,44,36,29,22,14,7,0
    };
    // 方波閃爍：上半週亮、下半週滅；亮度壓到 40/255，不刺眼
    unsigned int v = (ledPhase < 128) ? 40 : 0;
    ledcWrite(0, v);
}

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
// 顏色（全用 TFT_eSPI 標準色）
static const uint16_t HDR_BG  = 0x2400;   // 深藍表頭
static const uint16_t WX_BG   = 0x0A1A2E;  // 氣象條深藍底
static const uint16_t C_TITLE = TFT_YELLOW;
static const uint16_t C_UPD   = TFT_CYAN;
static const uint16_t C_WEST  = TFT_GREEN;
static const uint16_t C_EAST  = TFT_MAGENTA;
static const uint16_t C_LIST  = TFT_WHITE;
static const uint16_t C_WX    = TFT_WHITE;
static const uint16_t C_WX_DIM= 0x5555;

// 時鐘格式：MM/DD HH:MM（NTP 未同步時印 "NTP pending"）
String clockStr() {
    struct tm t;
    if (!getLocalTime(&t, 2000)) return String("NTP pending");
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d",
             t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    return String(buf);
}

// 局部重繪：drawScreen() 全清後必須重印，其餘只有字串變化才重畫
static bool clockNeedsRedraw = true;   // 全清後第一次一定要畫
void clockTick(unsigned long now) {
    if (now - lastClockCheckMs < CLOCK_CHECK_MS) return;
    lastClockCheckMs = now;
    char newStr[16];
    String s = clockStr();
    s.toCharArray(newStr, sizeof(newStr));
    if (strcmp(newStr, lastClockStr) == 0 && !clockNeedsRedraw) return;
    clockNeedsRedraw = false;
    strncpy(lastClockStr, newStr, sizeof(lastClockStr) - 1);
    lastClockStr[sizeof(lastClockStr) - 1] = '\0';
    tft.fillRect(CLOCK_X, CLOCK_Y - 1, CLOCK_W, CLOCK_H + 2, HDR_BG);
    tft.setTextColor(TFT_WHITE, HDR_BG);
    tft.setTextSize(1);
    tft.setCursor(CLOCK_X, CLOCK_Y);
    tft.print(lastClockStr);
}


void drawScreen() {
    tft.fillScreen(TFT_BLACK);
    int W = tft.width();

    // ── 表頭 ──
    tft.fillRect(0, 0, W, 26, HDR_BG);
    tft.setTextSize(1);
    tft.setTextColor(C_TITLE, HDR_BG);
    tft.setCursor(4, 3);
    tft.println(F("Sun-Sun Park - Bus ETA"));
    tft.setTextColor(C_UPD, HDR_BG);
    tft.setCursor(4, 15);
    tft.print(F("Upd: "));
    tft.print(busData.getLastUpdateTime());

    // 全清後，讓下一次 clockTick 一定要重印時鐘
    clockNeedsRedraw = true;

    // ── 統計行 ──
    int west = busData.getCount(0);
    int east = busData.getCount(1);
    tft.setTextColor(C_WEST, TFT_BLACK);
    tft.setCursor(4, 28);
    tft.printf("WEST %-2d   ", west);
    tft.setTextColor(C_EAST, TFT_BLACK);
    tft.print(F("EAST "));
    tft.print(east);

    // ── 車次清單（東西向各取前 8 筆，依序上下排列）──
    int y = 44;
    int step = 13;
    int shown = 0;
    for (int d = 0; d < 2 && shown < 12; d++) {
        int n = busData.getCount(d);
        for (int i = 0; i < n && shown < 12; i++) {
            const BusInfo* b = busData.getBusAt(d, i);
            if (!b || y + step > tft.height()) break;
            tft.setTextColor(d == 0 ? C_WEST : C_EAST, TFT_BLACK);
            tft.setCursor(4, y);
            tft.printf("%-9s %-3d", b->routeName.c_str(), b->estimateSecs / 60);
            tft.setTextColor(C_LIST, TFT_BLACK);
            tft.printf("  %s", b->timeStatus.c_str());
            y += step;
            shown++;
        }
    }
    if (shown == 0) {
        tft.setTextColor(0x5555, TFT_BLACK);
        tft.setCursor(4, 44);
        tft.println(F("No bus data"));
    }
}

void drawWxStrip() {
    int y = tft.height() - 38;
    tft.fillRect(0, y, tft.width(), 38, WX_BG);
    tft.drawLine(0, y, tft.width(), y, 0x4080F0);

    if (!wx.isValid()) {
        tft.setTextColor(C_WX_DIM, WX_BG);
        tft.setTextSize(1);
        tft.setCursor(4, y + 4);
        tft.println(F("Weather: --   H--/L--"));
        tft.setCursor(4, y + 18);
        tft.println(F("AQI --  Hum --%  Wind --"));
        return;
    }

    tft.setTextSize(1);
    tft.setTextColor(C_WX, WX_BG);
    tft.setCursor(4, y + 4);
    tft.printf("%3dC  %s  H%d/L%d",
               wx.getTemp(), wx.getWxText().c_str(), wx.getWxHigh(), wx.getWxLow());
    tft.setCursor(4, y + 18);
    tft.printf("AQI %-3d  Hum %-3d%%  Wind %d",
               wx.getAqi(), wx.getHum(), wx.getWind());
}

void doFetchAndShow() {
    // 一次抓回所有新新公園站牌（22 支 StopUID 各自回報各自路線）
    String all = tdx.fetchBusETAByName(TARGET_STOP_NAME);
    busData.parseTDXResponse(all);
    drawScreen();
    drawWxStrip();
    clockNeedsRedraw = true;
    Serial.printf("[TDX] fetched, total=%d\n", busData.getTotalCount());
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== HW-950 Bus ETA ===");
#ifdef FW_VERSION
    Serial.printf("[info] fw=%s build=%s\n", FW_VERSION, __DATE__);
#endif

    // strapping
    pinMode(12, OUTPUT); digitalWrite(12, LOW);

    // 呼吸燈（綠燈 17 PWM）
    ledBegin();

    // 【關鍵】隔離觸控 & SD CS，釋放 MISO
    pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
    pinMode(SD_CS,    OUTPUT); digitalWrite(SD_CS,    HIGH);
    delay(20);

    Serial.println("TFT_eSPI init...");
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

#ifdef FW_VERSION
    tft.setTextColor(C_WX_DIM, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0, tft.height() - 8);
    tft.printf("%s %s", FW_VERSION, __DATE__);
#endif

    // 連線中：快呼吸
    ledState = LED_FAST;
    tft.setTextColor(C_UPD, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(5, 5);
    tft.println(F("Connecting WiFi..."));

    // Wi-Fi
    wifi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (!wifi.isConnected() && tries < 60) {
        wifi.loop();
        tries++;
        delay(2000);
        ledTick(millis());
        if (tries % 5 == 0) {
            tft.fillRect(5, 20, tft.width() - 10, 16, TFT_BLACK);
            tft.setCursor(5, 20);
            tft.printf("retry %d ...\n", tries / 5);
        }
    }

    if (wifi.isConnected()) {
        String ip = wifi.getIP();
        tft.fillRect(5, 20, tft.width() - 10, 16, TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(5, 20);
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
            drawWxStrip();
            Serial.printf("[WX] initial fetch OK, temp=%d aqi=%d\n", wx.getTemp(), wx.getAqi());
        } else {
            Serial.println("[WX] initial fetch failed, retry in 10 min");
        }
        lastWxMs = millis();

        // 上電先抓一次公車
        doFetchAndShow();
        lastFetchMs = millis();
        ledState = LED_SLOW;
    } else {
        tft.fillRect(5, 20, tft.width() - 10, 16, TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(5, 20);
        tft.println(F("WiFi failed"));
        Serial.println("[FATAL] Wi-Fi 連線失敗");
        ledState = LED_OFF;
        return;
    }
}

void loop() {
    wifi.loop();
    ledTick(millis());
    clockTick(millis());

    // 斷線：燈熄
    if (!wifi.isConnected() && ledState != LED_OFF) {
        ledState = LED_FAST;
    } else if (wifi.isConnected()) {
        ledState = LED_SLOW;
    }

    // 定期 NTP 重對時
    if (wifi.isConnected() && millis() - lastNtpMs >= NTP_REFRESH_MS) {
        syncTime();
    }

    unsigned long now = millis();
    if (wifi.isConnected() && now - lastFetchMs >= (unsigned long)REFRESH_INTERVAL_S * 1000) {
        lastFetchMs = now;
        doFetchAndShow();
    }
    // 气象/AQI 与公交车分开计时
    if (wifi.isConnected() && now - lastWxMs >= WX_REFRESH_MS) {
        lastWxMs = now;
        if (wx.fetchAll()) {
            // 局部重绘气象条 + 时钟（公交车 60s 会全清一次，所以这里也重置时钟 flag）
            drawWxStrip();
            clockNeedsRedraw = true;
            clockTick(millis());
        }
    }
    delay(50);
}
