/*
 * DisplayManager.h - TFT 顯示管理
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "BusData.h"

class DisplayManager {
private:
    TFT_eSPI tft;
    bool initialized = false;
    uint16_t bgColor = TFT_BLACK;
    uint16_t textColor = TFT_WHITE;
    uint16_t accentColor = TFT_CYAN;
    uint16_t headerColor = TFT_YELLOW;
    uint16_t errorColor = TFT_RED;
    uint16_t busColor = TFT_GREEN;

    // UI 區域定義
    struct UIRegions {
        int headerY = 0;
        int headerH = 35;
        int contentY = 35;
        int contentH = 0;
        int footerY = 0;
        int footerH = 30;
        int refreshBtnX = 0;
        int refreshBtnY = 0;
        int refreshBtnW = 80;
        int refreshBtnH = 25;
        int scrollUpX = 0;
        int scrollUpY = 0;
        int scrollUpW = 40;
        int scrollUpH = 40;
        int scrollDownX = 0;
        int scrollDownY = 0;
        int scrollDownW = 40;
        int scrollDownH = 40;
    } ui;

    void calcUIRegions();
    void drawHeader(const String& title, const String& subtitle = "");
    void drawHeader(const String& title, const String& subtitle, uint16_t titleColor);
    void drawFooter(const String& status = "");
    void drawFooter(const String& status, uint16_t color);
    void drawRefreshButton(bool pressed = false);
    void drawScrollButtons();
    void drawBusItem(int index, const BusInfo& bus, int y, bool highlight = false);

public:
    DisplayManager();
    ~DisplayManager();

    bool begin();
    void showStartupScreen();
    void showIP(const String& ip);
    void showBusData(const BusData& data);
    void showError(const String& error);
    void showConnecting(const String& ssid);
    void showMessage(const String& title, const String& msg);

    // 觸控檢測
    bool isRefreshButtonPressed(int x, int y);
    bool isScrollUpPressed(int x, int y);
    bool isScrollDownPressed(int x, int y);

    // 低層存取 (進階用)
    TFT_eSPI& getTFT() { return tft; }
    int width() { return tft.width(); }
    int height() { return tft.height(); }
};

#endif