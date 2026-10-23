/*
 * UIManager.h - 頁面佈局與繪製
 *
 * Splash: SD卡狀態 + Firmware版本
 * Page 0: ETA (3筆/頁, 5秒自動捲動)
 * Page 1: Slideshow 全螢幕 (無跑馬燈)
 * 全 ASCII，中文字畫不出。
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "BusData.h"
#include "WxManager.h"
#include "TouchManager.h"
#include "WiFiManager.h"
#include "TDXClient.h"
#include "SDManager.h"
#include "LightSensor.h"
#include "Slideshow.h"

enum Page {
    PAGE_DASHBOARD = 0,
    PAGE_SLIDESHOW = 1,
    PAGE_CALIBRATION = 2
};

struct LayoutMetrics {
    int screenW = 320;
    int screenH = 240;

    // 天氣列 40px (保留顯示天氣/時鐘)
    int wxH = 40;
    int wxIconX = 8, wxIconY = 8, wxIconSize = 22;
    int wxTempX = 34;
    int wxTextX = 66;
    int wxClockX = 176; // 時鐘左對齊

    // 公車區 40..240
    int listTop = 40;
    int listBottom = 240;
    int rowH = 44;         // 單行高度
    int dirLabelH = 20;    // 方向標籤高度
    int visibleRows = 4;   // 4 列 × 44 = 176px + dirLabelH 20 = 196 < 200

    // Slideshow 相片全螢幕
    int photoTop = 40;
    int photoH = 200;
};

class UIManager {
private:
    TFT_eSPI* tft;
    BusData* busData;
    WxManager* wx;
    WiFiManager* wifi;
    TDXClient* tdx;
    SDManager* sd;
    LightSensor* ldr;
    LayoutMetrics m;

    // 顏色
    static const uint16_t BG       = 0x010B;  // #020617
    static const uint16_t CARD     = 0x0719;  // #0E1223
    static const uint16_t WX_BG    = 0x052F;  // #0A1A2E
    static const uint16_t C_WEST   = 0x07E0;  // #22C55E
    static const uint16_t C_EAST   = 0xF800;  // #EF4444
    static const uint16_t C_WARN   = 0xFD20;  // #F59E0B
    static const uint16_t C_FG     = 0xFFFF;  // #F8FAFC
    static const uint16_t C_DIM    = 0x5ACB;  // #94A3B8
    static const uint16_t C_BRDR   = 0x1CE7;  // #334155
    static const uint16_t C_SUN    = 0xFD20;  // #F5C518 偏琥珀
    static const uint16_t C_RAIN   = 0x79F0;  // #6AB0FF
    static const uint16_t C_THST   = 0xFF20;
    static const uint16_t C_SD_OK  = 0x07E0;  // 綠 - SD OK
    static const uint16_t C_SD_NO  = 0xF800;  // 紅 - SD無

    // 時鐘/日期快取
    char lastClock[12] = "";
    char lastDate[16]  = "";
    bool clockNeedsRedraw = true;
    unsigned long lastClockCheckMs = 0;
    static const unsigned long CLOCK_CHECK_MS = 500UL;

    // 公車分頁自動滾動 (5 秒切換 1 頁)
    int busPageIndex = 0;
    int totalBusPages = 0;
    static const int ITEMS_PER_PAGE = 3;  // 每頁顯示筆數
    unsigned long lastBusPageMs = 0;
    static const unsigned long BUS_PAGE_MS = 5000UL;  // 5 秒切頁
    int totalRows = 0;

    // Splash 狀態
    bool showSplash = true;
    unsigned long splashStartMs = 0;
    static const unsigned long SPLASH_DURATION_MS = 3000UL; // 3秒顯示

    // TJpg_Decoder 回呼
    static bool jpegCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data);
    static UIManager* s_active;
    Slideshow* slideshow = nullptr;

    // helpers
    void calcMetrics();
    String clockStr() const;
    String dateStr() const;
    void drawWeatherBar();
    void drawWxIcon(int x, int y, int size);
    void routeDisp(const BusInfo* b, char* out, int outSize);
    void drawBusList();
    void drawBusRow(const BusInfo* b, int y, int dir);
    void drawSlideshow();
    void drawPhotoPlaceholder();
    void updateBusScroll();
    void drawHeaderClock();
    void drawSplashScreen();

public:
    // Calibration page (needs to be called from main.cpp loop)
    void drawCalibrationPage(int currentPoint, int16_t* calX, int16_t* calY);

    UIManager();
    ~UIManager();

    void begin(TFT_eSPI* t, BusData* b, WxManager* w,
               WiFiManager* wf, TDXClient* td, SDManager* s, LightSensor* l);

    void drawPage(Page p);

    // 狀態控制
    void setShowSplash(bool show) { showSplash = show; splashStartMs = millis(); }
    bool isShowingSplash() const { return showSplash; }
    unsigned long getSplashStartMs() const { return splashStartMs; }

    // 局部更新
    void updateClock();
    void updateBusList();
    void setSlideshowSource(Slideshow* s) { slideshow = s; }
    void updateSlideshowPhoto();

    // 觸控 hit-test
    bool inCalibratePoint(int x, int y, int idx) const;
    bool inCalibrateDone(int x, int y) const;
    bool inCalibrateCancel(int x, int y) const;

    // dirty flags
public:
    bool fullRedrawNeeded = true;
    bool busListDirty = false;
    bool weatherDirty = false;
    bool photoDirty = false;

    void requestFullRedraw() { fullRedrawNeeded = true; }
    void setBusDirty() { busListDirty = true; }
    void setWeatherDirty() { weatherDirty = true; busListDirty = true; }
    void setPhotoDirty() { photoDirty = true; }
};

#endif