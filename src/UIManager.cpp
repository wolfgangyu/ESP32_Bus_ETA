/*
 * UIManager.cpp - 頁面繪製（v1.0 spec 實作）
 * 全 ASCII：西向 WEST / 東向 EAST / 即將到 ARR / 分 min / 濕 RH
 * 中文字畫不出（Flash 容不下字表），只支援英文字。
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
}

void UIManager::calcMetrics() {
    if (!tft) return;
    m.screenW = tft->width();   // 320
    m.screenH = tft->height();  // 240
    m.listBottom = m.screenH;
    m.marqueeY = m.screenH - m.marqueeH;
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

// ── 天氣圖標（drawCircle + drawLine，size≈22px）──
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
            // 太陽在後（CLOUD 才有）
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
// TDX 回傳的 routeName 可能是 "06S"（shuttle）或 "306Shuttle"；上屏統一簡寫
void UIManager::routeDisp(const BusInfo* b, char* out, int outSize) {
    const char* src = b->routeName.c_str();
    // 抓前導數字
    char num[8]; int n = 0;
    while (src[n] && isdigit(src[n]) && n < 7) num[n++] = src[n];
    num[n] = 0;
    // 判斷 shuttle：數字後接 S/sh/Sh/Shuttle 都算
    char after[3] = { src[n], src[n+1], 0 };
    bool shuttle = (after[0] == 'S' || after[0] == 's') ||
                   (src[n] == 's' && src[n+1] == 'h') ||
                   (strcasestr(src, "shuttle") != nullptr);
    if (shuttle) snprintf(out, outSize, "%ssh", num);
    else         snprintf(out, outSize, "%s", num);
}

// ── 天氣條（含 (i) 鈕、右上時鐘日期）──
void UIManager::drawWeatherBar() {
    tft->fillRect(0, 0, m.screenW, m.wxH, WX_BG);
    tft->drawLine(0, m.wxH - 1, m.screenW, m.wxH - 1, C_BRDR);

    // 圖標（22x22，置左 8px）
    drawWxIcon(m.wxIconX, m.wxIconY, m.wxIconSize);

    // 溫度（16px 放大一級）
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

    // 狀態文字：上方短名（GLCD 8px 最小），下方 AQI/RH（GLCD 8px 最小，跟短名同小）
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
        tft->setTextSize(0);  // GLCD 8px 短名
        tft->setTextColor(C_FG, WX_BG);
        tft->setCursor(tx, 4);
        tft->print(shortName);
        tft->setTextSize(0);  // GLCD 8px AQI/RH
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

    // 時鐘 + 日期（右側，(i) 鈕左方）
    drawHeaderClock();

    // (i) 按鈕
    drawInfoButton();
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

    // 時鐘 26px（上，放大一級）、日期 GLCD 8px（下），(i) 鈕左邊
    // 固定左對齊 wxClockX；擦除寬固定，字寬估算不準時也不亂
    tft->fillRect(m.wxClockX - 4, 0, m.infoBtnX - m.wxClockX + 8, m.wxH - 1, WX_BG);
    tft->setTextSize(2);  // FONT4 26px 時鐘
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(m.wxClockX, 0);
    tft->print(lastClock);
    tft->setTextSize(0);  // GLCD 8px 日期（含星期，完整 "SUN 10/18"）
    tft->setTextColor(C_DIM, WX_BG);
    tft->setCursor(m.wxClockX, 26);
    tft->print(lastDate);
}

void UIManager::drawInfoButton() {
    int cx = m.infoBtnX + m.infoBtnSize / 2;
    int cy = m.infoBtnY + m.infoBtnSize / 2;
    tft->drawCircle(cx, cy, m.infoBtnSize / 2, C_DIM);
    tft->setTextSize(1);
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(cx - 2, cy - 4);
    tft->print('i');
}

// ── 公車滿幅 ──
void UIManager::drawBusList() {
    tft->fillRect(0, m.listTop, m.screenW, m.listBottom - m.listTop, BG);

    // 中間分隔線
    tft->drawLine(m.screenW/2, m.listTop, m.screenW/2, m.listBottom, C_BRDR);

    int half = m.screenW / 2;
    int y = m.listTop + 4;

    // 方向標籤：WEST/EAST（GLCD 8px 小字）+ 右側「N 班」（16px dim，收後不蓋字）
    int westN = busData ? busData->getCount(0) : 0;
    int eastN = busData ? busData->getCount(1) : 0;
    tft->setTextSize(0);  // GLCD 8px 標籤
    tft->setTextColor(C_WEST, BG);
    tft->setCursor(8, y + 6);
    tft->print(F("WEST"));
    tft->setTextColor(C_EAST, BG);
    tft->setCursor(half + 8, y + 6);
    tft->print(F("EAST"));
    // 班數（GLCD 8px，跟「306sh」同寬度坐標 + 同 y，留 6px 空隙不壓到）
    tft->setTextColor(C_DIM, BG);
    char nbuf[4];
    snprintf(nbuf, sizeof(nbuf), "%d", westN);
    tft->setCursor(half - 8 - 6 - 6, y + 6);
    tft->print(nbuf);
    snprintf(nbuf, sizeof(nbuf), "%d", eastN);
    tft->setCursor(m.screenW - 8 - 6 - 6, y + 6);
    tft->print(nbuf);

    y += m.dirLabelH;
    int totalWest = busData ? busData->getCount(0) : 0;
    int totalEast = busData ? busData->getCount(1) : 0;
    totalRows = totalWest + totalEast;

    // 計算單列可見數量
    int visible = m.visibleRows;

    // 西向列（左半，獨立捲動）
    int wMaxScroll = max(0, totalWest - visible);
    int wStart = (wMaxScroll > 0) ? (busScrollOffset % (wMaxScroll + 1)) : 0;
    int wShown = 0;
    for (int i = 0; i < totalWest && wShown < visible; i++) {
        int idx = (wStart + i) % totalWest;
        const BusInfo* b = busData->getBusAt(0, idx);
        if (!b) break;
        int rowY = y + wShown * m.rowH;
        if (rowY + m.rowH > m.listBottom) break;
        drawBusRow(b, rowY, 0);
        wShown++;
    }
    // 東向列（右半，獨立捲動，用同一個 busScrollOffset 節奏）
    int eMaxScroll = max(0, totalEast - visible);
    int eStart = (eMaxScroll > 0) ? (busScrollOffset % (eMaxScroll + 1)) : 0;
    int eShown = 0;
    for (int i = 0; i < totalEast && eShown < visible; i++) {
        int idx = (eStart + i) % totalEast;
        const BusInfo* b = busData->getBusAt(1, idx);
        if (!b) break;
        int rowY = y + eShown * m.rowH;
        if (rowY + m.rowH > m.listBottom) break;
        drawBusRow(b, rowY, 1);
        eShown++;
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
    int colW = m.screenW / 2;  // 160

    // 路線名（FONT4 26px，06S → 06sh），靠左
    char rn[8];
    routeDisp(b, rn, sizeof(rn));
    tft->setTextSize(2);
    tft->setTextColor(c, BG);
    tft->setCursor(x0, y + 6);
    tft->print(rn);

    // ETA 分鐘（大字 26px，跟路線名拉開；用亮色避免被背景吃掉）
    int mins = b->estimateSecs / 60;
    if (mins < 1 && b->estimateSecs > 0) mins = 1;
    uint16_t etaC = (mins <= 5 && b->estimateSecs > 0) ? C_WARN : C_FG;
    tft->setTextSize(2);
    tft->setTextColor(etaC, BG);
    int etaX = x0 + 70;  // 跟路線名留足 24px 空隙
    tft->setCursor(etaX, y + 6);
    tft->print(String(mins));
    // min 小字（GLCD 8px）放分鐘數字正下方，y+6+18=24（行高 44px，距下邊 20px 不壓到下一列）
    tft->setTextSize(0);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(etaX, y + 24);
    tft->print(F("min"));
}

// ── 自動捲動：東西向各各獨立，10s 捲 1 列，停 5s ──
void UIManager::updateBusScroll() {
    int totalWest = busData ? busData->getCount(0) : 0;
    int totalEast = busData ? busData->getCount(1) : 0;
    int maxScroll = max(totalWest, totalEast) - m.visibleRows;
    if (maxScroll <= 0) {
        busScrollOffset = 0;
        scrollAtTop = scrollAtBottom = false;
        totalRows = totalWest + totalEast;
        return;
    }
    totalRows = totalWest + totalEast;

    unsigned long now = millis();
    if (now - lastBusScrollMs < BUS_SCROLL_MS) return;

    if (scrollAtTop) {
        if (now - lastBusScrollMs >= BUS_PAUSE_MS) {
            scrollAtTop = false;
            busScrollOffset = 1;
            lastBusScrollMs = now;
            busListDirty = true;
        }
        return;
    }
    if (scrollAtBottom) {
        if (now - lastBusScrollMs >= BUS_PAUSE_MS) {
            scrollAtBottom = false;
            busScrollOffset = 0;  // 回到頂端循環
            lastBusScrollMs = now;
            busListDirty = true;
        }
        return;
    }
    busScrollOffset++;
    if (busScrollOffset >= maxScroll) {
        busScrollOffset = maxScroll;
        scrollAtBottom = true;
    }
    lastBusScrollMs = now;
    busListDirty = true;
}

// ── Slideshow ──
void UIManager::drawSlideshow() {
    tft->fillScreen(BG);
    drawWeatherBar();
    // 相片區
    tft->fillRect(0, m.photoTop, m.screenW, m.photoH, 0x0A1A2E);
    updateSlideshowPhoto();
    // 跑馬燈（半透明深色底 + 公車文字）
    drawMarquee();
}

void UIManager::updateSlideshowPhoto() {
    if (slideshow && slideshow->getPhotoCount() > 0) {
        uint8_t* buf = nullptr; size_t len = 0; int w = 0, h = 0;
        if (slideshow->getCurrentPhoto(buf, len, w, h)) {
            // 置中、縮放至 maxW=320 maxH=170（留 30px 給跑馬燈下緣）
            int maxW = m.screenW - 4;
            int maxH = m.photoH - 4;
            float s = min((float)maxW / w, (float)maxH / h);
            int dw = (int)(w * s), dh = (int)(h * s);
            int dx = (m.screenW - dw) / 2;
            int dy = m.photoTop + (m.photoH - dh) / 2;
            TJpgDec.setJpgScale(1);
            TJpgDec.setCallback(jpegCallback);
            TJpgDec.drawJpg(dx, dy, buf, len);
        }
    } else {
        drawPhotoPlaceholder();
    }
    photoDirty = false;
}

void UIManager::drawPhotoPlaceholder() {
    int cx = m.screenW / 2;
    int cy = m.photoTop + m.photoH / 2;
    int w = 300, h = 180;
    tft->drawRect(cx - w/2, cy - h/2, w, h, C_BRDR);
    tft->setTextSize(1);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(cx - w/2 + 10, cy - 8);
    tft->print(F("NO PHOTO"));
    tft->setCursor(cx - w/2 + 10, cy + 8);
    tft->print(F("Put JPG on SD"));
}

void UIManager::drawMarquee() {
    int y = m.marqueeY;
    // 55% 深色底（無 GPU blur）
    tft->fillRect(0, y, m.screenW, m.marqueeH, 0x010B);
    tft->drawLine(0, y, m.screenW, y, C_BRDR);

    // 收集公車文字
    String w, e;
    if (busData) {
        int wn = busData->getCount(0);
        for (int i = 0; i < wn && i < 4; i++) {
            const BusInfo* b = busData->getBusAt(0, i);
            if (b) {
                w += b->routeName;
                w += " ";
                w += String(b->estimateSecs / 60);
                w += "min ";
            }
        }
        int en = busData->getCount(1);
        for (int i = 0; i < en && i < 4; i++) {
            const BusInfo* b = busData->getBusAt(1, i);
            if (b) {
                e += b->routeName;
                e += " ";
                e += String(b->estimateSecs / 60);
                e += "min ";
            }
        }
    }
    tft->setTextSize(1);
    tft->setTextColor(C_WEST, BG);
    tft->setCursor(6, y + 8);
    tft->print(F("W ")); tft->print(w.length() ? w : F("-"));
    tft->setTextColor(C_EAST, BG);
    tft->setCursor(6, y + 20);
    tft->print(F("E ")); tft->print(e.length() ? e : F("-"));
    marqueeDirty = false;
}

// ── 系統頁 ──
void UIManager::drawSystemPage() {
    tft->fillScreen(BG);
    tft->setTextSize(2);
    tft->setTextColor(C_FG, BG);
    tft->setCursor(10, 8);
    tft->print(F("SYSTEM"));

    // 右上 × 鈕
    int cx = m.screenW - 16, cy = 16;
    tft->drawCircle(cx, cy, 10, C_DIM);
    tft->drawLine(cx - 5, cy - 5, cx + 5, cy + 5, C_DIM);
    tft->drawLine(cx + 5, cy - 5, cx - 5, cy + 5, C_DIM);

    struct Row { const char* k; char v[24]; bool ok; };
    Row rows[12];
    int n = 0;

    rows[n].k = "WiFi";
    if (wifi && wifi->isConnected()) {
        String tmp = "ON " + wifi->getIP();
        tmp.toCharArray(rows[n].v, sizeof(rows[n].v));
        rows[n].ok = true;
    } else {
        strcpy(rows[n].v, "OFF");
        rows[n].ok = false;
    }
    n++;

    rows[n].k = "WiFi MAC";
    {
        String macStr = WiFi.macAddress();  // "04:B2:47:84:6B:A0" 格式
        strncpy(rows[n].v, macStr.c_str(), sizeof(rows[n].v) - 1);
        rows[n].v[sizeof(rows[n].v) - 1] = 0;
        rows[n].ok = macStr.length() > 0;
    }
    n++;

    rows[n].k = "TDX Calls";
    snprintf(rows[n].v, sizeof(rows[n].v), "%d today", tdx ? tdx->getCallCount() : 0);
    rows[n].ok = false;
    n++;

    rows[n].k = "Data Used";
    snprintf(rows[n].v, sizeof(rows[n].v), "%.1f KB", tdx ? tdx->getDataUsedKB() : 0.0f);
    rows[n].ok = false;
    n++;

    rows[n].k = "SD Card";
    if (sd && sd->isMounted()) {
        snprintf(rows[n].v, sizeof(rows[n].v), "OK %.1f GB", sd->getTotalGB());
        rows[n].ok = true;
    } else {
        strcpy(rows[n].v, "NO CARD");
        rows[n].ok = false;
    }
    n++;

    rows[n].k = "Photos";
    snprintf(rows[n].v, sizeof(rows[n].v), "%d / 50", sd ? sd->countPhotos() : 0);
    rows[n].ok = false;
    n++;

    rows[n].k = "Backlight";
    snprintf(rows[n].v, sizeof(rows[n].v), "AUTO %d%%", ldr ? (int)ldr->getDuty() * 100 / 255 : 0);
    rows[n].ok = false;
    n++;

    rows[n].k = "Firmware";
    snprintf(rows[n].v, sizeof(rows[n].v), "%s %s", FW_VERSION, __DATE__);
    rows[n].ok = false;
    n++;

    rows[n].k = "TDX Stop";
    snprintf(rows[n].v, sizeof(rows[n].v), "%s", TARGET_STOP_NAME);
    rows[n].ok = false;
    n++;

    int y = 40;
    for (int i = 0; i < n && y < m.screenH - 8; i++) {
        tft->setTextSize(1);
        tft->setTextColor(C_DIM, BG);
        tft->setCursor(10, y);
        tft->print(rows[i].k);
        tft->setTextColor(rows[i].ok ? C_WEST : C_FG, BG);
        int vlen = strlen(rows[i].v) * 6;
        tft->setCursor(m.screenW - 10 - vlen, y);
        tft->print(rows[i].v);
        y += 16;
        if (i < n - 1) tft->drawLine(0, y + 4, m.screenW, y + 4, 0x141D30);
        y += 8;
    }
    fullRedrawNeeded = false;
}

// ── 主入口 ──
void UIManager::drawPage(Page p) {
    switch (p) {
        case PAGE_DASHBOARD:
            tft->fillScreen(BG);
            drawWeatherBar();
            drawBusList();
            clockNeedsRedraw = true;
            lastBusScrollMs = millis();
            scrollAtTop = true;
            busScrollOffset = 0;
            fullRedrawNeeded = false;
            break;
        case PAGE_SLIDESHOW:
            drawSlideshow();
            fullRedrawNeeded = false;
            break;
        case PAGE_SYSTEM:
            drawSystemPage();
            fullRedrawNeeded = false;
            break;
    }
}

// ── 局部時鐘更新（只改天氣條右側）──
void UIManager::updateClock() {
    unsigned long now = millis();
    if (now - lastClockCheckMs < CLOCK_CHECK_MS) return;
    lastClockCheckMs = now;
    drawHeaderClock();
}

void UIManager::updateBusList() {
    updateBusScroll();
    if (busListDirty) drawBusList();
}

void UIManager::updateMarquee() {
    if (marqueeDirty) drawMarquee();
}

// ── 觸控 hit-test ──
bool UIManager::inInfoBtn(int x, int y) const {
    return x >= m.infoBtnX && x <= m.infoBtnX + m.infoBtnSize &&
           y >= m.infoBtnY && y <= m.infoBtnY + m.infoBtnSize;
}
bool UIManager::inSystemClose(int x, int y) const {
    int cx = m.screenW - 16, cy = 16;
    return (x-cx)*(x-cx) + (y-cy)*(y-cy) <= 100;
}
bool UIManager::inPhoto(int x, int y) const {
    return y >= m.photoTop && y < m.marqueeY;
}
bool UIManager::inBusRow(int x, int y, int& idx) const {
    if (y < m.listTop + m.dirLabelH || y >= m.listBottom) return false;
    idx = (y - m.listTop - m.dirLabelH) / m.rowH + busScrollOffset;
    return idx >= 0 && idx < totalRows;
}