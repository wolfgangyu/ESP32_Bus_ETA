/*
 * DisplayManager.cpp - TFT 顯示管理實作
 */

#include "DisplayManager.h"
#include "config.h"

DisplayManager::DisplayManager() {}

DisplayManager::~DisplayManager() {}

bool DisplayManager::begin() {
    // 【關鍵】init 前把觸控 CS=33、SD CS=5 拉高隔離，防 MISO 匯流排被扯低
    // （44 號實測證實：這步缺了螢幕會全白）
    #define TOUCH_CS_ISOLATE 33
    #define SD_CS_ISOLATE    5
    pinMode(TOUCH_CS_ISOLATE, OUTPUT); digitalWrite(TOUCH_CS_ISOLATE, HIGH);
    pinMode(SD_CS_ISOLATE,    OUTPUT); digitalWrite(SD_CS_ISOLATE,    HIGH);
    delay(20);
    #undef TOUCH_CS_ISOLATE
    #undef SD_CS_ISOLATE

    tft.init();

    // 根據驅動晶片設定旋轉
    #if defined(GC9A01_DRIVER)
        tft.setRotation(0);  // 圓形螢幕通常不需旋轉
    #elif defined(ILI9341_DRIVER)
        tft.setRotation(1);  // 2.4" 橫向
    #elif defined(ST7789_DRIVER)
        tft.setRotation(1);
    #elif defined(ILI9486_DRIVER) || defined(ST7796_DRIVER)
        tft.setRotation(1);  // 3.5" 橫向
    #else
        tft.setRotation(1);
    #endif

    tft.fillScreen(bgColor);
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);

    // 背光控制
    #ifdef TFT_BL
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    #endif

    calcUIRegions();
    initialized = true;

    #ifdef DEBUG_SERIAL
    Serial.printf("TFT 初始化完成: %dx%d\n", tft.width(), tft.height());
    #endif

    return true;
}

void DisplayManager::calcUIRegions() {
    int w = tft.width();
    int h = tft.height();

    ui.headerH = 35;
    ui.headerY = 0;
    ui.contentY = ui.headerH;
    ui.contentH = h - ui.headerH - 30;
    ui.footerH = 30;
    ui.footerY = h - ui.footerH;

    // 重新整理按鈕 - 右下角
    ui.refreshBtnW = 80;
    ui.refreshBtnH = 25;
    ui.refreshBtnX = w - ui.refreshBtnW - 5;
    ui.refreshBtnY = ui.footerY + 2;

    // 捲動按鈕 - 右側
    ui.scrollUpW = 40;
    ui.scrollUpH = 40;
    ui.scrollUpX = w - ui.scrollUpW - 5;
    ui.scrollUpY = ui.contentY + 5;

    ui.scrollDownW = 40;
    ui.scrollDownH = 40;
    ui.scrollDownX = w - ui.scrollDownW - 5;
    ui.scrollDownY = ui.footerY - ui.scrollDownH - 5;
}

void DisplayManager::showStartupScreen() {
    if (!initialized) return;

    tft.fillScreen(bgColor);

    // 標題
    tft.setTextColor(headerColor, bgColor);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ESP32 Bus ETA", tft.width()/2, 60);

    // 副標題
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    tft.drawString("公車到站提示系統", tft.width()/2, 95);

    #ifdef SHOW_HELLO_WORLD
    tft.setTextSize(2);
    tft.setTextColor(accentColor, bgColor);
    tft.drawString("Hello World!", tft.width()/2, 140);
    #endif

    // 版本資訊
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, bgColor);
    tft.drawString("Initializing...", tft.width()/2, tft.height() - 30);

    #ifdef DEBUG_SERIAL
    Serial.println("顯示開機畫面");
    #endif
}

void DisplayManager::showIP(const String& ip) {
    if (!initialized) return;

    tft.fillScreen(bgColor);
    drawHeader("WiFi 已連線", "IP 位址");
    tft.setTextColor(accentColor, bgColor);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(ip, tft.width()/2, tft.height()/2);
    drawFooter("繼續載入公車資料...");
}

void DisplayManager::showConnecting(const String& ssid) {
    if (!initialized) return;

    tft.fillScreen(bgColor);
    drawHeader("連線中...", "WiFi: " + ssid);
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("請稍候", tft.width()/2, tft.height()/2);
    drawFooter("");
}

void DisplayManager::showBusData(const BusData& data) {
    if (!initialized) return;

    tft.fillScreen(bgColor);

    // 表頭
    String subtitle = "站牌: " + String(TARGET_STOP_NAME);
    if (data.getCount() > 0) {
        subtitle += "  |  更新: " + data.getLastUpdateTime();
    }
    drawHeader("公車即時到站", subtitle);

    // 內容區
    int itemHeight = 35;
    int startY = ui.contentY + 5;
    int maxItems = min(MAX_BUS_DISPLAY, (ui.contentH - 10) / itemHeight);
    int displayCount = min(data.getVisibleCount(), maxItems);

    for (int i = 0; i < displayCount; i++) {
        const BusInfo* bus = data.getBusAt(i);
        if (bus) {
            int y = startY + i * itemHeight;
            drawBusItem(i, *bus, y);
        }
    }

    // 空資料提示
    if (data.getCount() == 0) {
        tft.setTextColor(TFT_DARKGREY, bgColor);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("查無公車資料", tft.width()/2, tft.height()/2);
    }

    // 底部狀態列與按鈕
    String status = "共 " + String(data.getCount()) + " 筆";
    if (data.getVisibleCount() > maxItems) {
        status += " (捲動查看更多)";
    }
    drawFooter(status);
    drawRefreshButton();
    drawScrollButtons();
}

void DisplayManager::drawBusItem(int index, const BusInfo& bus, int y, bool highlight) {
    uint16_t itemBg = highlight ? TFT_NAVY : bgColor;
    uint16_t routeColor = busColor;
    uint16_t timeColor = TFT_ORANGE;
    uint16_t destColor = TFT_WHITE;

    // 背景
    tft.fillRect(5, y, tft.width() - 10, 32, itemBg);

    // 路線號碼 - 左側大字
    tft.setTextColor(routeColor, itemBg);
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(bus.routeName, 10, y + 16);

    // 預計到站時間 - 右側
    tft.setTextColor(timeColor, itemBg);
    tft.setTextSize(1);
    tft.setTextDatum(MR_DATUM);
    String etaText = bus.estimateTime + " 分";
    if (bus.estimateTime.toInt() == 0) etaText = "進站中";
    tft.drawString(etaText, tft.width() - 10, y + 16);

    // 去向 - 下方小字
    tft.setTextColor(destColor, itemBg);
    tft.setTextSize(1);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(bus.destination, 10, y + 30);

    // 車輛狀態指示器
    if (bus.isLowFloor) {
        tft.setTextColor(TFT_BLUE, itemBg);
        tft.setTextSize(1);
        tft.setTextDatum(MR_DATUM);
        tft.drawString("無障礙", tft.width() - 90, y + 16);
    }
}

void DisplayManager::showError(const String& error) {
    if (!initialized) return;

    tft.fillScreen(bgColor);
    drawHeader("錯誤", "", errorColor);
    tft.setTextColor(errorColor, bgColor);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);

    // 將長錯誤訊息換行顯示
    int y = tft.height()/2 - 20;
    String msg = error;
    while (msg.length() > 0) {
        String line = msg.substring(0, min(30, (int)msg.length()));
        tft.drawString(line, tft.width()/2, y);
        y += 20;
        msg = msg.substring(min(30, (int)msg.length()));
    }

    drawFooter("點擊螢幕重試", errorColor);
}

void DisplayManager::showMessage(const String& title, const String& msg) {
    if (!initialized) return;

    tft.fillScreen(bgColor);
    drawHeader(title);
    tft.setTextColor(textColor, bgColor);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(msg, tft.width()/2, tft.height()/2);
    drawFooter("");
}

void DisplayManager::drawHeader(const String& title, const String& subtitle) {
    drawHeader(title, subtitle, headerColor);
}

void DisplayManager::drawFooter(const String& status) {
    drawFooter(status, 0);
}

void DisplayManager::drawHeader(const String& title, const String& subtitle, uint16_t titleColor) {
    tft.fillRect(0, ui.headerY, tft.width(), ui.headerH, TFT_NAVY);

    tft.setTextColor(titleColor != 0 ? titleColor : headerColor, TFT_NAVY);
    tft.setTextSize(2);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(title, 10, ui.headerY + ui.headerH/2);

    if (subtitle.length() > 0) {
        tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
        tft.setTextSize(1);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(subtitle, 10, ui.headerY + ui.headerH - 8);
    }
}

void DisplayManager::drawFooter(const String& status, uint16_t color) {
    tft.fillRect(0, ui.footerY, tft.width(), ui.footerH, TFT_NAVY);

    if (status.length() > 0) {
        tft.setTextColor(color != 0 ? color : TFT_LIGHTGREY, TFT_NAVY);
        tft.setTextSize(1);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(status, 10, ui.footerY + ui.footerH/2);
    }
}

void DisplayManager::drawRefreshButton(bool pressed) {
    uint16_t btnColor = pressed ? TFT_DARKCYAN : accentColor;
    uint16_t txtColor = TFT_BLACK;

    tft.fillRoundRect(ui.refreshBtnX, ui.refreshBtnY, ui.refreshBtnW, ui.refreshBtnH, 5, btnColor);
    tft.setTextColor(txtColor, btnColor);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("重新整理", ui.refreshBtnX + ui.refreshBtnW/2, ui.refreshBtnY + ui.refreshBtnH/2);
}

void DisplayManager::drawScrollButtons() {
    // 向上
    tft.fillRoundRect(ui.scrollUpX, ui.scrollUpY, ui.scrollUpW, ui.scrollUpH, 5, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("/\\", ui.scrollUpX + ui.scrollUpW/2, ui.scrollUpY + ui.scrollUpH/2);

    // 向下
    tft.fillRoundRect(ui.scrollDownX, ui.scrollDownY, ui.scrollDownW, ui.scrollDownH, 5, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(2);
    tft.drawString("\\/", ui.scrollDownX + ui.scrollDownW/2, ui.scrollDownY + ui.scrollDownH/2);
}

bool DisplayManager::isRefreshButtonPressed(int x, int y) {
    return (x >= ui.refreshBtnX && x <= ui.refreshBtnX + ui.refreshBtnW &&
            y >= ui.refreshBtnY && y <= ui.refreshBtnY + ui.refreshBtnH);
}

bool DisplayManager::isScrollUpPressed(int x, int y) {
    return (x >= ui.scrollUpX && x <= ui.scrollUpX + ui.scrollUpW &&
            y >= ui.scrollUpY && y <= ui.scrollUpY + ui.scrollUpH);
}

bool DisplayManager::isScrollDownPressed(int x, int y) {
    return (x >= ui.scrollDownX && x <= ui.scrollDownX + ui.scrollDownW &&
            y >= ui.scrollDownY && y <= ui.scrollDownY + ui.scrollDownH);
}