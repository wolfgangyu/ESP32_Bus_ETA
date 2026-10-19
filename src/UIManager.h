/*
 * UIManager.h - 頁面佈局與繪製（v1.0 spec）
 *
 * 主畫面：天氣條(含圖標+時鐘日期+(i)) + 公車滿幅(34px ETA 自動捲動) + 無 footer
 * 系統頁：9 行資訊 + 右上角 × 返回（(i) 進入）
 * Slideshow：天氣條 + 相片全幅 200px + 公車跑馬燈半透明疊照片下緣
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
    PAGE_SYSTEM = 2
};

struct LayoutMetrics {
    int screenW = 320;
    int screenH = 240;

    // 天氣條 40px
    int wxH = 40;
    int wxIconX = 8, wxIconY = 8, wxIconSize = 22;
    int wxTempX = 34;
    int wxTextX = 66;   // 預設（未使用，短名實際 x=80）
    int wxClockX = 176; // 時鐘左對齊，避開 AQI 文字，(i) 鈕在 294
    int infoBtnX = 294, infoBtnY = 10, infoBtnSize = 20;

    // 公車滿幅 40..240
    int listTop = 40;
    int listBottom = 240;   // 主畫面公車區到最底（無 footer）
    int rowH = 44;         // 26px 行 + 8px min = 34px，留 10px 間距
    int dirLabelH = 20;    // 方向標籤 16px 單行
    int visibleRows = 4;   // 4 列 × 44 = 176px + dirLabelH 20 = 196 < 200
    int maxRows = 8;       // 單向最多顯示列數

    // Slideshow 相片 40..240
    int photoTop = 40;
    int photoH = 200;
    int marqueeH = 34;
    int marqueeY = 240 - 34;
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
    static const uint16_t BG      = 0x010B;  // #020617
    static const uint16_t CARD    = 0x0719;  // #0E1223
    static const uint16_t WX_BG   = 0x052F;  // #0A1A2E
    static const uint16_t C_WEST  = 0x07E0;  // #22C55E
    static const uint16_t C_EAST  = 0xF800;  // #EF4444
    static const uint16_t C_WARN  = 0xFD20;  // #F59E0B
    static const uint16_t C_FG    = 0xFFFF;  // #F8FAFC
    static const uint16_t C_DIM   = 0x5ACB;  // #94A3B8
    static const uint16_t C_BRDR  = 0x1CE7;  // #334155
    static const uint16_t C_SUN   = 0xFD20;  // #F5C518 偏琥珀
    static const uint16_t C_RAIN  = 0x79F0;  // #6AB0FF
    static const uint16_t C_THST  = 0xFF20;

    // 時鐘/日期快取
    char lastClock[12] = "";
    char lastDate[16]  = "";
    bool clockNeedsRedraw = true;
    unsigned long lastClockCheckMs = 0;
    static const unsigned long CLOCK_CHECK_MS = 500UL;

    // 公车自动滚动（10 秒捲動 1 列）
    int busScrollOffset = 0;
    unsigned long lastBusScrollMs = 0;
    static const unsigned long BUS_SCROLL_MS = 10000UL;
    static const unsigned long BUS_PAUSE_MS = 5000UL;
    bool scrollAtTop = true;
    bool scrollAtBottom = false;
    int totalRows = 0;

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
    void drawInfoButton();
    void drawSlideshow();
    void drawPhotoPlaceholder();
    void drawMarquee();
    void drawSystemPage();
    void updateBusScroll();
    void drawHeaderClock();

public:
    UIManager();
    ~UIManager();

    void begin(TFT_eSPI* t, BusData* b, WxManager* w,
               WiFiManager* wf, TDXClient* td, SDManager* s, LightSensor* l);

    void drawPage(Page p);

    // 局部更新
    void updateClock();
    void updateBusList();
    void updateMarquee();
    void setSlideshowSource(Slideshow* s) { slideshow = s; }
    void updateSlideshowPhoto();

    // 觸控 hit-test
    bool inInfoBtn(int x, int y) const;
    bool inSystemClose(int x, int y) const;
    bool inPhoto(int x, int y) const;
    bool inBusRow(int x, int y, int& idx) const;

    // dirty flags
public:
    bool fullRedrawNeeded = true;
    bool busListDirty = false;
    bool weatherDirty = false;
    bool marqueeDirty = false;
    bool photoDirty = false;

    void requestFullRedraw() { fullRedrawNeeded = true; }
    void setBusDirty() { busListDirty = true; }
    void setWeatherDirty() { weatherDirty = true; busListDirty = true; }
    void setMarqueeDirty() { marqueeDirty = true; }
    void setPhotoDirty() { photoDirty = true; }
};

#endif