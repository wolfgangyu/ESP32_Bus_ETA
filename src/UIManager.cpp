/*
 * UIManager.cpp - 頁面繪製
 *
 * Splash: SD卡狀態 + Firmware版本 (3秒)
 * Page 0: ETA (3筆/頁, 5秒自動捲動)
 * Page 1: Slideshow 全螢幕 (無跑馬燈)
 * 全 ASCII，中文字畫不出。
 */

#include "UIManager.h"
#include <time.h>
#include <WiFi.h>

UIManager* UIManager::s_active = nullptr;

UIManager::UIManager() : tft(nullptr), busData(nullptr), wx(nullptr),
    wifi(nullptr), tdx(nullptr), sd(nullptr), ldr(nullptr) {}

UIManager::~UIManager() {}

void UIManager::begin(TFT_eSPI* t, BusData* b, WxManager* w,
                      WiFiManager* wf, TDXClient* td, SDManager* s, LightSensor* l) {
    tft = t; busData = b; wx = w;
    wifi = wf; tdx = td; sd = s; ldr = l;
    calcMetrics();
    s_active = this;
    showSplash = true;
    splashStartMs = millis();
}

void UIManager::calcMetrics() {
    if (!tft) return;
    m.screenW = tft->width();   // 320
    m.screenH = tft->height();  // 240
    m.listBottom = m.screenH;
    m.photoH = m.screenH - m.photoTop;
    m.visibleRows = (m.listBottom - m.listTop - m.dirLabelH) / m.rowH;
}

bool UIManager::jpegCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data) {
    if (data && s_active && s_active->tft) {
        s_active->tft->pushImage(x, y, w, h, data);
    }
    return true;
}

String UIManager::clockStr() const {
    struct tm t;
    if (!getLocalTime(&t, 1000)) return String("--:--");
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
}

String UIManager::dateStr() const {
    struct tm t;
    if (!getLocalTime(&t, 1000)) return String("--");
    const char* wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    char buf[16];
    snprintf(buf, sizeof(buf), "%s %02d/%02d", wd[t.tm_wday], t.tm_mon + 1, t.tm_mday);
    return String(buf);
}

// ── 天氣圖標 ──
void UIManager::drawWxIcon(int x, int y, int size) {
    if (!wx || !wx->isValid()) return;
    int cx = x + size / 2, cy = y + size / 2;
    int r = size / 3;
    switch (wx->getWxIconType()) {
        case WxManager::ICON_SUN:
            tft->fillCircle(cx, cy, r, C_SUN);
            for (int a = 0; a < 360; a += 45) {
                int a1 = a * 3.14159f / 180;
                tft->drawLine(cx + (int)(cos(a1)*r*1.4), cy + (int)(sin(a1)*r*1.4),
                              cx + (int)(cos(a1)*r*1.9), cy + (int)(sin(a1)*r*1.9), C_SUN);
            }
            break;
        case WxManager::ICON_CLOUD:
        case WxManager::ICON_OVR: {
            uint16_t c = (wx->getWxIconType()==WxManager::ICON_CLOUD) ? C_FG : C_DIM;
            if (wx->getWxIconType()==WxManager::ICON_CLOUD)
                tft->fillCircle(cx - r/2, cy - r/2, r/2, C_SUN);
            tft->fillCircle(cx - r/3, cy, r, c);
            tft->fillCircle(cx + r/2, cy + 2, r*7/10, c);
            tft->fillCircle(cx + r/2 - 1, cy - 2, r*8/10, c);
            break;
        }
        case WxManager::ICON_RAIN:
            tft->fillCircle(cx - r/4, cy - r/4, r, C_DIM);
            tft->fillCircle(cx + r/2, cy - r/4, r*8/10, C_DIM);
            for (int i = -1; i <= 1; i++)
                tft->drawLine(cx + i*(r/2), cy + r/2, cx + i*(r/2) - 2, cy + r, C_RAIN);
            break;
        case WxManager::ICON_THST:
            tft->fillCircle(cx, cy - r/2, r, C_SUN);
            tft->drawLine(cx + 2, cy - 2, cx - 3, cy + r/2, C_THST);
            tft->drawLine(cx - 3, cy + r/2, cx + 4, cy + r/2, C_THST);
            tft->drawLine(cx + 4, cy + r/2, cx - 1, cy + r, C_THST);
            break;
        case WxManager::ICON_FOG:
            for (int i = 0; i < 3; i++)
                tft->drawLine(x, y + i*4 + 2, x + size, y + i*4 + 2, C_DIM);
            break;
        case WxManager::ICON_SNOW:
            tft->fillCircle(cx, cy, r, C_DIM);
            for (int i = 0; i < 3; i++)
                tft->drawCircle(x + 4 + i*(size/3), cy + r + 2, 1, C_FG);
            break;
        default:
            tft->drawRect(x + 4, y + size/2 - 1, size - 8, 2, C_DIM);
            break;
    }
}

// ── 路線顯示：06S → 06sh、306Shuttle → 306sh ──
void UIManager::routeDisp(const BusInfo* b, char* out, int outSize) {
    const char* src = b->routeName.c_str();
    char num[8]; int n = 0;
    while (src[n] && isdigit(src[n]) && n < 7) {
        num[n] = src[n];
        n++;
    }
    num[n] = 0;
    char after[3] = { src[n], src[n+1], 0 };
    bool shuttle = (after[0] == 'S' || after[0] == 's') ||
                   (src[n] == 's' && src[n+1] == 'h') ||
                   (strcasestr(src, "shuttle") != nullptr);
    if (shuttle) snprintf(out, outSize, "%ssh", num);
    else         snprintf(out, outSize, "%s", num);
}

// ── 天氣條（含時鐘日期）──
void UIManager::drawWeatherBar() {
    tft->fillRect(0, 0, m.screenW, m.wxH, WX_BG);
    tft->drawLine(0, m.wxH - 1, m.screenW, m.wxH - 1, C_BRDR);

    // 圖標
    drawWxIcon(m.wxIconX, m.wxIconY, m.wxIconSize);

    // 溫度
    tft->setTextSize(1);
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(m.wxTempX, m.wxH / 2 - 9);
    if (wx && wx->isValid()) {
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%dC", wx->getTemp());
        tft->print(tbuf);
    } else {
        tft->print(F("--"));
    }

    // 狀態文字
    int tx = 80;
    if (wx && wx->isValid()) {
        const char* shortName = "N/A";
        switch (wx->getWxIconType()) {
            case WxManager::ICON_SUN: shortName = "SUN"; break;
            case WxManager::ICON_CLOUD: shortName = "CLOUDY"; break;
            case WxManager::ICON_OVR: shortName = "OVR"; break;
            case WxManager::ICON_RAIN: shortName = "RAIN"; break;
            case WxManager::ICON_THST: shortName = "THST"; break;
            case WxManager::ICON_FOG: shortName = "FOG"; break;
            case WxManager::ICON_SNOW: shortName = "SNOW"; break;
        }
        tft->setTextSize(0);
        tft->setTextColor(C_FG, WX_BG);
        tft->setCursor(tx, 4);
        tft->print(shortName);
        tft->setTextSize(0);
        tft->setTextColor(C_DIM, WX_BG);
        tft->setCursor(tx, 14);
        tft->printf("AQI %d RH%d", wx->getAqi(), wx->getHum());
    } else {
        tft->setTextSize(0);
        tft->setTextColor(C_DIM, WX_BG);
        tft->setCursor(tx, 4);
        tft->print(F("WX --"));
        tft->setCursor(tx, 14);
        tft->print(F("H--/L--"));
    }

    // 時鐘 + 日期
    drawHeaderClock();
    weatherDirty = false;
}

void UIManager::drawHeaderClock() {
    String c = clockStr();
    String d = dateStr();
    char nc[12], nd[16];
    c.toCharArray(nc, sizeof(nc));
    d.toCharArray(nd, sizeof(nd));
    bool ch = strcmp(nc, lastClock) != 0;
    bool dh = strcmp(nd, lastDate) != 0;
    if (!ch && !dh && !clockNeedsRedraw) return;

    strncpy(lastClock, nc, sizeof(lastClock) - 1); lastClock[11] = 0;
    strncpy(lastDate, nd, sizeof(lastDate) - 1); lastDate[15] = 0;
    clockNeedsRedraw = false;

    tft->fillRect(m.wxClockX - 4, 0, m.screenW - m.wxClockX + 4, m.wxH - 1, WX_BG);
    tft->setTextSize(2);
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(m.wxClockX, 0);
    tft->print(lastClock);
    tft->setTextSize(0);
    tft->setTextColor(C_DIM, WX_BG);
    tft->setCursor(m.wxClockX, 26);
    tft->print(lastDate);
}

// ── Splash 畫面 ──
void UIManager::drawSplashScreen() {
    tft->fillScreen(BG);

    // 標題
    tft->setTextSize(2);
    tft->setTextColor(C_FG, BG);
    tft->setCursor(20, 30);
    tft->println(F("ESP32 Bus ETA"));

    tft->setTextSize(1);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(20, 60);
    tft->println(F("HW-950  ILI9341"));

    // SD 卡狀態
    tft->setCursor(20, 100);
    tft->print(F("SD Card: "));
    if (sd && sd->isMounted()) {
        tft->setTextColor(C_SD_OK, BG);
        tft->printf("OK  %.1f GB", sd->getTotalGB());
    } else {
        tft->setTextColor(C_SD_NO, BG);
        tft->print(F("NOT FOUND"));
    }

    // Firmware 版本
    tft->setCursor(20, 130);
    tft->setTextColor(C_DIM, BG);
    tft->printf("FW: %s", FW_VERSION);
    tft->setCursor(20, 150);
    tft->printf("Build: %s", __DATE__);

    // 底部提示
    tft->setTextSize(0);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(20, 210);
    tft->print(F("Loading..."));
}

// ── 公車列表 (3筆/頁) ──
void UIManager::drawBusList() {
    tft->fillRect(0, m.listTop, m.screenW, m.listBottom - m.listTop, BG);

    // 中間分隔線
    tft->drawLine(m.screenW/2, m.listTop, m.screenW/2, m.listBottom, C_BRDR);

    int half = m.screenW / 2;
    int y = m.listTop + 4;

    // 方向標籤
    int totalWest = busData ? busData->getCount(0) : 0;
    int totalEast = busData ? busData->getCount(1) : 0;
    totalRows = totalWest + totalEast;

    tft->setTextSize(0);
    tft->setTextColor(C_WEST, BG);
    tft->setCursor(8, y + 6);
    tft->print(F("WEST"));
    tft->setTextColor(C_EAST, BG);
    tft->setCursor(half + 8, y + 6);
    tft->print(F("EAST"));

    // 班數
    tft->setTextColor(C_DIM, BG);
    char nbuf[4];
    snprintf(nbuf, sizeof(nbuf), "%d", totalWest);
    tft->setCursor(half - 8 - 6 - 6, y + 6);
    tft->print(nbuf);
    snprintf(nbuf, sizeof(nbuf), "%d", totalEast);
    tft->setCursor(m.screenW - 8 - 6 - 6, y + 6);
    tft->print(nbuf);

    // 頁碼
    if (totalBusPages > 1) {
        tft->setTextSize(0);
        tft->setTextColor(C_DIM, BG);
        tft->setCursor(m.screenW - 50, m.listTop + 6);
        tft->printf("%d/%d", busPageIndex + 1, totalBusPages);
    }

    y += m.dirLabelH;

    // 計算總頁數
    int totalItems = totalWest + totalEast;
    totalBusPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalBusPages == 0) totalBusPages = 1;

    // 修正頁碼
    if (busPageIndex >= totalBusPages) busPageIndex = 0;

    // 計算這一頁的起始與結束索引
    int startIdx = busPageIndex * ITEMS_PER_PAGE;
    int endIdx = min(startIdx + ITEMS_PER_PAGE, totalItems);

    int shown = 0;
    for (int idx = startIdx; idx < endIdx && shown < ITEMS_PER_PAGE; idx++) {
        int dir = 0;
        int localIdx = idx;
        if (localIdx >= totalWest) {
            dir = 1;
            localIdx -= totalWest;
        }
        const BusInfo* b = busData->getBusAt(dir, localIdx);
        if (!b) break;
        int rowY = y + shown * m.rowH;
        if (rowY + m.rowH > m.listBottom) break;
        drawBusRow(b, rowY, dir);
        shown++;
    }

    if (totalRows == 0) {
        tft->setTextColor(C_DIM, BG);
        tft->setTextSize(1);
        tft->setCursor(10, y + 10);
        tft->print(F("No bus data"));
    }
    busListDirty = false;
}

void UIManager::drawBusRow(const BusInfo* b, int y, int dir) {
    uint16_t c = (dir == 0) ? C_WEST : C_EAST;
    int x0 = (dir == 0) ? 8 : 8 + m.screenW/2;

    // 路線名
    char rn[8];
    routeDisp(b, rn, sizeof(rn));
    tft->setTextSize(2);
    tft->setTextColor(c, BG);
    tft->setCursor(x0, y + 6);
    tft->print(rn);

    // ETA 分鐘
    int mins = b->estimateSecs / 60;
    if (mins < 1 && b->estimateSecs > 0) mins = 1;
    uint16_t etaC = (mins <= 5 && b->estimateSecs > 0) ? C_WARN : C_FG;
    tft->setTextSize(2);
    tft->setTextColor(etaC, BG);
    int etaX = x0 + 70;
    tft->setCursor(etaX, y + 6);
    tft->print(String(mins));
    tft->setTextSize(0);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(etaX, y + 24);
    tft->print(F("min"));
}

// ── 公車分頁自動切換：5 秒切 1 頁 ──
void UIManager::updateBusScroll() {
    int totalWest = busData ? busData->getCount(0) : 0;
    int totalEast = busData ? busData->getCount(1) : 0;
    int totalItems = totalWest + totalEast;

    int newTotalPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (newTotalPages == 0) newTotalPages = 1;
    totalBusPages = newTotalPages;

    if (totalBusPages <= 1) {
        busPageIndex = 0;
        totalRows = totalItems;
        return;
    }
    totalRows = totalItems;

    unsigned long now = millis();
    if (now - lastBusPageMs < BUS_PAGE_MS) return;

    // 切換到下一頁
    busPageIndex = (busPageIndex + 1) % totalBusPages;
    lastBusPageMs = now;
    busListDirty = true;
}

// ── Slideshow ──
void UIManager::drawSlideshow() {
    tft->fillScreen(BG);
    // 相片區 - 全螢幕 (無天氣列、無跑馬燈)
    tft->fillRect(0, 0, m.screenW, m.screenH, 0x0A1A2E);
    updateSlideshowPhoto();
}

void UIManager::updateSlideshowPhoto() {
    if (photoDirty) {
        if (slideshow && slideshow->getPhotoCount() > 0) {
            uint8_t* buf = nullptr; size_t len = 0; int w = 0, h = 0;
            if (slideshow->getCurrentPhoto(buf, len, w, h)) {
                // 置中、縮放至全螢幕 (320x240)
                int maxW = m.screenW - 4;
                int maxH = m.screenH - 4;
                float s = min((float)maxW / w, (float)maxH / h);
                int dw = (int)(w * s), dh = (int)(h * s);
                int dx = (m.screenW - dw) / 2;
                int dy = (m.screenH - dh) / 2;
                TJpgDec.setJpgScale(1);
                TJpgDec.setCallback(jpegCallback);
                TJpgDec.drawJpg(dx, dy, buf, len);
            }
        } else {
            drawPhotoPlaceholder();
        }
        photoDirty = false;
    }
}

void UIManager::drawPhotoPlaceholder() {
    int cx = m.screenW / 2;
    int cy = m.screenH / 2;
    int w = 300, h = 180;
    tft->drawRect(cx - w/2, cy - h/2, w, h, C_BRDR);
    tft->setTextSize(1);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(cx - w/2 + 10, cy - 8);
    tft->print(F("NO PHOTO"));
    tft->setCursor(cx - w/2 + 10, cy + 8);
    tft->print(F("Put JPG on SD"));
}

void UIManager::drawPage(Page p) {
    switch (p) {
        case PAGE_DASHBOARD:
            if (showSplash) {
                drawSplashScreen();
            } else {
                tft->fillScreen(BG);
                drawWeatherBar();
                drawBusList();
                clockNeedsRedraw = true;
                lastBusPageMs = millis();
                busPageIndex = 0;
                busListDirty = true;
            }
            fullRedrawNeeded = false;
            break;
        case PAGE_SLIDESHOW:
            drawSlideshow();
            fullRedrawNeeded = false;
            break;
        case PAGE_CALIBRATION:
            fullRedrawNeeded = false;
            break;
    }
}

void UIManager::updateClock() {
    unsigned long now = millis();
    if (now - lastClockCheckMs < CLOCK_CHECK_MS) return;
    lastClockCheckMs = now;
    drawHeaderClock();
}

void UIManager::updateBusList() {
    updateBusScroll();  // 每輪都要檢查捲動計時
    if (busListDirty) drawBusList();
}

// ── 觸控 hit-test ──
bool UIManager::inCalibratePoint(int x, int y, int idx) const {
    const int size = 30;
    int px[4], py[4];
    int margin = 20;
    px[0] = margin; py[0] = margin;
    px[1] = m.screenW - margin; py[1] = margin;
    px[2] = margin; py[2] = m.screenH - margin;
    px[3] = m.screenW - margin; py[3] = m.screenH - margin;
    if (idx < 0 || idx > 3) return false;
    return x >= px[idx] - size/2 && x <= px[idx] + size/2 &&
           y >= py[idx] - size/2 && y <= py[idx] + size/2;
}

bool UIManager::inCalibrateDone(int x, int y) const {
    int btnW = 80, btnH = 35;
    int bx = m.screenW - btnW - 10;
    int by = m.screenH - btnH - 10;
    return x >= bx && x <= bx + btnW && y >= by && y <= by + btnH;
}

bool UIManager::inCalibrateCancel(int x, int y) const {
    int btnW = 80, btnH = 35;
    int bx = 10;
    int by = m.screenH - btnH - 10;
    return x >= bx && x <= bx + btnW && y >= by && y <= by + btnH;
}

// ── 校準頁面繪製 ──
void UIManager::drawCalibrationPage(int currentPoint, int16_t* calX, int16_t* calY) {
    tft->fillScreen(BG);

    // 標題
    tft->setTextSize(2);
    tft->setTextColor(C_FG, BG);
    tft->setCursor(10, 10);
    tft->print(F("TOUCH CALIBRATION"));

    // 說明
    tft->setTextSize(1);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(10, 40);
    tft->printf("Press point %d/4", currentPoint + 1);

    // 4 個校準點位置
    const int size = 30;
    int px[4], py[4];
    int margin = 20;
    px[0] = margin; py[0] = margin;
    px[1] = m.screenW - margin; py[1] = margin;
    px[2] = margin; py[2] = m.screenH - margin;
    px[3] = m.screenW - margin; py[3] = m.screenH - margin;

    // 畫出所有點
    for (int i = 0; i < 4; i++) {
        uint16_t color;
        if (i < currentPoint) color = C_WEST;
        else if (i == currentPoint) color = C_WARN;
        else color = C_DIM;

        tft->drawCircle(px[i], py[i], size/2, color);
        tft->drawCircle(px[i], py[i], size/2 - 4, color);
        tft->drawLine(px[i] - size/4, py[i], px[i] + size/4, py[i], color);
        tft->drawLine(px[i], py[i] - size/4, px[i], py[i] + size/4, color);

        tft->setTextSize(1);
        tft->setTextColor(color, BG);
        char label[4];
        snprintf(label, sizeof(label), "%d", i + 1);
        int tx = px[i] - 3;
        int ty = py[i] + size/2 + 6;
        tft->setCursor(tx, ty);
        tft->print(label);
    }

    // 如果已完成全部 4 點，顯示校準值並等待確認
    if (currentPoint >= 4) {
        tft->setTextSize(1);
        tft->setTextColor(C_FG, BG);
        tft->setCursor(10, 70);
        tft->print(F("Calibration complete:"));
        tft->setCursor(10, 88);
        tft->printf("X0=%d Y0=%d", calX[0], calY[0]);
        tft->setCursor(10, 104);
        tft->printf("X1=%d Y1=%d", calX[1], calY[1]);
        tft->setCursor(10, 120);
        tft->printf("X2=%d Y2=%d", calX[2], calY[2]);
        tft->setCursor(10, 136);
        tft->printf("X3=%d Y3=%d", calX[3], calY[3]);

        // Done 按鈕
        int btnW = 80, btnH = 35;
        int bx = m.screenW - btnW - 10;
        int by = m.screenH - btnH - 10;
        tft->fillRoundRect(bx, by, btnW, btnH, 6, C_WEST);
        tft->setTextSize(1);
        tft->setTextColor(BG, C_WEST);
        tft->setCursor(bx + 18, by + 10);
        tft->print(F("SAVE"));

        // Cancel 按鈕
        bx = 10;
        tft->fillRoundRect(bx, by, btnW, btnH, 6, C_EAST);
        tft->setTextColor(BG, C_EAST);
        tft->setCursor(bx + 12, by + 10);
        tft->print(F("CANCEL"));
    }
}