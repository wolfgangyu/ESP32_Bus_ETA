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
    // 初始化翻頁計時器，避免首次資料載入後立即翻頁
    lastBusPageMsWest = millis();
    lastBusPageMsEast = millis();
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
    while (src[n] && isdigit(src[n]) && n < 7) {
        num[n] = src[n];
        n++;
    }
    num[n] = 0;
    // 判斷 shuttle：數字後接 S/sh/Sh/Shuttle 都算
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

    // 圖標（22x22，置左 8px）
    drawWxIcon(m.wxIconX, m.wxIconY, m.wxIconSize);

    // 溫度（18px）
    tft->setTextSize(2);
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(m.wxTempX, m.wxH / 2 - 11);
    if (wx && wx->isValid()) {
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%dC", wx->getTemp());
        tft->print(tbuf);
    } else {
        tft->print(F("--"));
    }

    // 狀態文字（GLCD 8px 兩行）：
    //   第 1 行：短名 + AQI + RH
    //   第 2 行：最低/最高溫 (L/H)
    const int tx = 80;
    tft->setTextSize(0);
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
        tft->setTextColor(C_FG, WX_BG);
        tft->setCursor(tx, 5);
        tft->printf("%s  AQI %d  RH%d", shortName, wx->getAqi(), wx->getHum());

        tft->setTextColor(C_DIM, WX_BG);
        tft->setCursor(tx, 22);
        tft->printf("L%d  H%d", wx->getWxLow(), wx->getWxHigh());
    } else {
        tft->setTextColor(C_DIM, WX_BG);
        tft->setCursor(tx, 5);
        tft->print(F("WX --"));
        tft->setCursor(tx, 22);
        tft->print(F("L--  H--"));
    }

    // 時鐘 + 日期（右側）
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

    // 時鐘 24px（上）、日期 GLCD 8px（下）
    tft->fillRect(m.screenW - 80, 0, 78, m.wxH - 1, WX_BG);
    tft->setTextSize(2);  // 24px 時鐘
    tft->setTextColor(C_FG, WX_BG);
    tft->setCursor(m.screenW - 76, 4);   // 距頂端 4px
    tft->print(lastClock);
    tft->setTextSize(0);  // GLCD 8px 日期（含星期，完整 "SUN 10/18"）
    tft->setTextColor(C_DIM, WX_BG);
    tft->setCursor(m.screenW - 76, 26);  // 距頂端 26px
    tft->print(lastDate);
}

// ── 公車列表 (3筆/頁，西向/東向獨立分頁) ──
void UIManager::drawBusList() {
    tft->fillRect(0, m.listTop, m.screenW, m.listBottom - m.listTop, BG);

    // 夜間暫停：01:00-05:00 不抓 TDX，顯示提示（保留最後一次資料）
    if (nightPause) {
        int cx = m.screenW / 2;
        int cy = m.listTop + (m.listBottom - m.listTop) / 2;
        tft->setTextSize(2);
        tft->setTextColor(C_DIM, BG);
        tft->setCursor(cx - 66, cy - 16);
        tft->print(F("NIGHT PAUSE"));
        tft->setTextSize(1);
        tft->setTextColor(C_BRDR, BG);
        tft->setCursor(cx - 60, cy + 12);
        tft->printf("resume %02d:00", nightResumeHour);
        busListDirty = false;
        return;
    }

    // 中間分隔線
    tft->drawLine(m.screenW/2, m.listTop, m.screenW/2, m.listBottom, C_BRDR);

    int half = m.screenW / 2;
    int y = m.listTop + 4;

    // 方向標籤
    int totalWest = busData ? busData->getCount(0) : 0;
    int totalEast = busData ? busData->getCount(1) : 0;
    totalRows = totalWest + totalEast;

    // 計算總頁數（必須在印標頭前算好）
    newTotalPagesWest = (totalWest + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (newTotalPagesWest == 0) newTotalPagesWest = 1;
    newTotalPagesEast = (totalEast + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (newTotalPagesEast == 0) newTotalPagesEast = 1;
    if (busPageIndexWest >= newTotalPagesWest) busPageIndexWest = 0;
    if (busPageIndexEast >= newTotalPagesEast) busPageIndexEast = 0;

    // ── 標頭：每欄各自「方向標籤 + 班數(白) + 頁碼(白, 右對齊)」──
    // GLCD 8px 每字 6px 寬
    tft->setTextSize(0);
    char nbuf[8];

    // 西向欄 (x 0..half)：WEST(8) 班數(44) 頁碼(右對齊 half-4-18)
    tft->setTextColor(C_WEST, BG);
    tft->setCursor(8, y + 6);
    tft->print(F("WEST"));
    tft->setTextColor(C_FG, BG);              // 班數改白色，避免被背景吃掉
    snprintf(nbuf, sizeof(nbuf), "%d", totalWest);
    tft->setCursor(44, y + 6);
    tft->print(nbuf);
    if (newTotalPagesWest > 1) {
        snprintf(nbuf, sizeof(nbuf), "%d/%d", busPageIndexWest + 1, newTotalPagesWest);
        tft->setCursor(half - 4 - (int)strlen(nbuf) * 6, y + 6);
        tft->print(nbuf);
    }

    // 東向欄 (x half..screenW)：EAST(half+8) 班數(half+44) 頁碼(右對齊 screenW-4-18)
    tft->setTextColor(C_EAST, BG);
    tft->setCursor(half + 8, y + 6);
    tft->print(F("EAST"));
    tft->setTextColor(C_FG, BG);
    snprintf(nbuf, sizeof(nbuf), "%d", totalEast);
    tft->setCursor(half + 44, y + 6);
    tft->print(nbuf);
    if (newTotalPagesEast > 1) {
        snprintf(nbuf, sizeof(nbuf), "%d/%d", busPageIndexEast + 1, newTotalPagesEast);
        tft->setCursor(m.screenW - 4 - (int)strlen(nbuf) * 6, y + 6);
        tft->print(nbuf);
    }

    y += m.dirLabelH;

    // 西向
    for (int shown = 0; shown < ITEMS_PER_PAGE; shown++) {
        int idx = busPageIndexWest * ITEMS_PER_PAGE + shown;
        if (idx >= totalWest) break;
        const BusInfo* b = busData->getBusAt(0, idx);
        if (!b) break;
        int rowY = m.listTop + 4 + m.dirLabelH + shown * m.rowH;
        if (rowY + m.rowH > m.listBottom) break;
        drawBusRow(b, rowY, 0);
    }

    // 東向
    for (int shown = 0; shown < ITEMS_PER_PAGE; shown++) {
        int idx = busPageIndexEast * ITEMS_PER_PAGE + shown;
        if (idx >= totalEast) break;
        const BusInfo* b = busData->getBusAt(1, idx);
        if (!b) break;
        int rowY = m.listTop + 4 + m.dirLabelH + shown * m.rowH;
        if (rowY + m.rowH > m.listBottom) break;
        drawBusRow(b, rowY, 1);
    }

    if (totalWest == 0 && totalEast == 0) {
        tft->setTextColor(C_DIM, BG);
        tft->setTextSize(1);
        tft->setCursor(10, m.listTop + 4 + m.dirLabelH + 10);
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

    // ETA 分鐘（大字 26px）+ 緊接在後的 "min"（GLCD 8px，同色，避免被背景吃掉）
    int mins = b->estimateSecs / 60;
    if (mins < 1 && b->estimateSecs > 0) mins = 1;
    uint16_t etaC = (mins <= 5 && b->estimateSecs > 0) ? C_WARN : C_FG;

    String num = String(mins);
    int etaX = x0 + 70;   // 跟路線名留足 24px 空隙
    tft->setTextSize(2);
    tft->setTextColor(etaC, BG);
    tft->setCursor(etaX, y + 6);
    tft->print(num);

    // "min" 貼在數字後方：textSize 2 每字 12px 寬
    int minX = etaX + num.length() * 12 + 3;
    tft->setTextSize(0);                       // GLCD 8px
    tft->setTextColor(etaC, BG);               // 與分鐘數同色
    tft->setCursor(minX, y + 12);              // 對齊數字的垂直中線
    tft->print(F("min"));
}

// ── 公車分頁自動切換：每方向獨立，>3筆才啟用，8秒切 1 頁 ──
void UIManager::updateBusScroll() {
    if (nightPause) return;   // 夜間暫停時不翻頁

    const int totalWest = busData ? busData->getCount(0) : 0;
    const int totalEast = busData ? busData->getCount(1) : 0;

    // 頁數（0-based 索引：0 = 第 1 頁）
    const int pagesWest = max(1, (totalWest + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    const int pagesEast = max(1, (totalEast + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);

    unsigned long now = millis();

    // 西向：>3 筆才自動翻頁，否則固定第 1 頁
    if (totalWest > ITEMS_PER_PAGE) {
        if (now - lastBusPageMsWest >= BUS_PAGE_MS) {
            lastBusPageMsWest = now;
            busPageIndexWest = (busPageIndexWest + 1) % pagesWest;
            busListDirty = true;
            Serial.printf("[UI] WEST page -> %d/%d\n", busPageIndexWest + 1, pagesWest);
        }
        if (busPageIndexWest >= pagesWest) busPageIndexWest = 0;  // 資料變少時夾回
    } else {
        busPageIndexWest = 0;
        lastBusPageMsWest = now;   // 保持計時器新鮮，資料長回來時不會立刻跳頁
    }

    // 東向：同上
    if (totalEast > ITEMS_PER_PAGE) {
        if (now - lastBusPageMsEast >= BUS_PAGE_MS) {
            lastBusPageMsEast = now;
            busPageIndexEast = (busPageIndexEast + 1) % pagesEast;
            busListDirty = true;
            Serial.printf("[UI] EAST page -> %d/%d\n", busPageIndexEast + 1, pagesEast);
        }
        if (busPageIndexEast >= pagesEast) busPageIndexEast = 0;
    } else {
        busPageIndexEast = 0;
        lastBusPageMsEast = now;
    }

    totalRows = totalWest + totalEast;
}

// ── Slideshow ──
void UIManager::drawSlideshow() {
    tft->fillScreen(BG);
    // 相片區 - 全螢幕 (無天氣列、無跑馬燈)
    tft->fillRect(0, 0, m.screenW, m.screenH, 0x0A1A2E);
    photoDirty = true;          // 進這頁一定要畫一次（否則無相片時會留空）
    updateSlideshowPhoto();
}

void UIManager::updateSlideshowPhoto() {
    if (!photoDirty) return;
    TJpgDec.setCallback(jpegCallback);

    // 全螢幕 320×240，串流解碼（不需要 200KB 緩衝）
    bool ok = slideshow && slideshow->drawCurrent(0, 0, m.screenW, m.screenH);
    if (!ok) drawPhotoPlaceholder();

    photoDirty = false;
}

void UIManager::drawPhotoPlaceholder() {
    int cx = m.screenW / 2;
    int cy = m.screenH / 2;
    int w = 300, h = 120;
    tft->drawRect(cx - w/2, cy - h/2, w, h, C_BRDR);
    tft->setTextSize(1);
    tft->setTextColor(C_DIM, BG);
    tft->setCursor(cx - w/2 + 12, cy - 30);
    tft->print(F("NO PHOTO"));
    tft->setTextColor(C_BRDR, BG);
    tft->setCursor(cx - w/2 + 12, cy - 6);
    tft->print(F("Create /photos on SD"));
    tft->setCursor(cx - w/2 + 12, cy + 10);
    tft->print(F("and put .jpg files in it"));
}

void UIManager::drawPage(Page p) {
    switch (p) {
        case PAGE_DASHBOARD:
            tft->fillScreen(BG);
            drawWeatherBar();
            drawBusList();
            clockNeedsRedraw = true;
            busListDirty = true;
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

        // 標籤
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
// ── 開機畫面 ──
void UIManager::drawSplashScreen() {
    static bool splashDrawn = false;
    if (splashDrawn) return;
    splashDrawn = true;   // 靜態內容只畫一次

    tft->fillScreen(BG);
    tft->setTextSize(2);
    tft->setTextColor(C_FG, BG);
    tft->setCursor(24, 28);
    tft->print(F("BUS ETA"));
    tft->drawFastHLine(24, 58, 120, C_WEST);

    // 顯示 IP
    if (wifi && wifi->isConnected()) {
        String ip = wifi->getIP();
        tft->setTextSize(1);
        tft->setTextColor(C_DIM, BG);
        tft->setCursor(24, 70);
        tft->print(F("IP: "));
        tft->print(ip);
    }

    // 顯示時間 (若 NTP 已同步)
    struct tm t;
    if (getLocalTime(&t, 0)) {
        tft->setTextSize(1);
        tft->setTextColor(C_FG, BG);
        tft->setCursor(24, 90);
        tft->printf("%04d-%02d-%02d %02d:%02d:%02d",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec);
    }
}
