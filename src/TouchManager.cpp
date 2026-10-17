/*
 * TouchManager.cpp - 觸控事件處理實作
 *
 * 使用 TFT_eSPI 內建觸控 API:
 *   uint8_t getTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600)
 *   回傳 1 代表有觸控，x/y 透過指標回傳
 */

#include "TouchManager.h"

TouchManager::TouchManager() {}

TouchManager::~TouchManager() {}

void TouchManager::begin(TFT_eSPI* tftPtr) {
    if (tftPtr) {
        tft = tftPtr;
    }
    initialized = true;
    wasPressed = false;
}

void TouchManager::setCalibration(int x0, int y0, int x1, int y1) {
    calX0 = x0;
    calY0 = y0;
    calX1 = x1;
    calY1 = y1;
}

void TouchManager::runCalibration() {
    // 觸控校準程序 - 顯示提示並引導用戶點擊四個角落
    // 此處為簡化版本，完整校準需結合 XPT2046 或 FT6236
    #ifdef DEBUG_SERIAL
    Serial.println("請進行觸控校準，依序點擊畫面四個角落...");
    #endif
}

TouchEvent TouchManager::read() {
    TouchEvent event;
    event.type = TOUCH_NONE;
    event.x = 0;
    event.y = 0;
    event.z = 0;
    event.time = millis();

    if (!initialized || !tft) return event;

    uint16_t rawX = 0, rawY = 0;
    // TFT_eSPI getTouch 回傳 uint8_t (0/1)，座標透過指標回傳
    // 簽名: uint8_t getTouch(uint16_t *x, uint16_t *y, uint16_t threshold = 600)
    bool pressed = tft->getTouch(&rawX, &rawY, 600);

    int x = pressed ? rawX : 0;
    int y = pressed ? rawY : 0;

    // 觸控校準 (若已設定)
    if (isCalibrated() && pressed) {
        // 線性插值轉換座標
        int width = tft->width();
        int height = tft->height();
        int calibratedX = map(x, calX0, calX1, 0, width);
        int calibratedY = map(y, calY0, calY1, 0, height);
        x = max(0, min(width - 1, calibratedX));
        y = max(0, min(height - 1, calibratedY));
    }

    event.x = x;
    event.y = y;
    event.z = pressed ? 1 : 0;  // 簡單的壓力指示

    // 狀態機處理觸控事件
    if (pressed && !wasPressed) {
        pressStartTime = millis();
        pressStartX = x;
        pressStartY = y;
    }

    if (pressed && wasPressed) {
        // 持續按下中
        int dx = abs(x - pressStartX);
        int dy = abs(y - pressStartY);

        if (millis() - pressStartTime > TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_HOLD;
        }
    }

    if (!pressed && wasPressed) {
        // 手指放開
        int pressDuration = millis() - pressStartTime;
        int dx = abs(x - pressStartX);
        int dy = abs(y - pressStartY);

        if (pressDuration <= TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_TAP;
        } else if (dy > dx && dy > MOVE_THRESHOLD) {
            if (y < pressStartY) {
                event.type = TOUCH_SWIPE_UP;
            } else {
                event.type = TOUCH_SWIPE_DOWN;
            }
        }

        event.x = x;
        event.y = y;
    }

    wasPressed = pressed;
    return event;
}