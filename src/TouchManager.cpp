/*
 * TouchManager.cpp - 觸控事件處理 (使用 TFT_eSPI 內建 XPT2046 支援 + 校準模式)
 *
 * HW-950 觸控 IC: XPT2046 (電阻式)
 * TFT_eSPI 使用 HSPI (GPIO 14, 13, 12) 連接 ILI9341
 * XPT2046 觸控使用 VSPI (GPIO 25, 32, 39, CS=33)
 * TFT_eSPI 庫會自動管理兩條 SPI 總線
 */

#include "TouchManager.h"
#include "config.h"
#include <TFT_eSPI.h>

TouchManager::TouchManager() {}

TouchManager::~TouchManager() {}

void TouchManager::begin() {
    // TFT_eSPI 在 tft.init() 時已自動初始化觸控控制器
    // 這裡只需標記已初始化
    initialized = true;
    wasPressed = false;
    Serial.println("[Touch] TFT_eSPI built-in XPT2046 ready");

    // 嘗試進行校準以檢查觸控是否工作
    // extern TFT_eSPI tft;
    // tft.calibrateTouch();  // 這會阻塞直到校準完成
}

TouchEvent TouchManager::read() {
    TouchEvent event;
    event.type = TOUCH_NONE;
    event.x = 0;
    event.y = 0;
    event.z = 0;
    event.time = millis();

    if (!initialized) return event;

    // 使用 TFT_eSPI 的觸控讀取
    extern TFT_eSPI tft;
    uint16_t x = 0, y = 0;
    // 使用較低的 threshold (500) 以提高靈敏度
    bool pressed = tft.getTouch(&x, &y, 500);

    // 測試用：也嘗試直接讀取原始觸控狀態
    // uint16_t rx = 0, ry = 0;
    // bool rpressed = tft.getTouchRaw(&rx, &ry);
    // Serial.printf("[TouchMgr] raw pressed=%d x=%d y=%d\n", rpressed, rx, ry);

    Serial.printf("[TouchMgr] getTouch pressed=%d x=%d y=%d\n", pressed, x, y);

    if (pressed && x <= 319 && y <= 239) {
        event.x = x;
        event.y = y;
        event.z = 1;
        Serial.printf("[TouchMgr] tft.getTouch=(%d,%d)\n", x, y);
    } else {
        event.x = 0;
        event.y = 0;
        event.z = 0;
    }

    // 狀態機判斷事件類型
    if (pressed && !wasPressed) {
        pressStartTime = millis();
        pressStartX = event.x;
        pressStartY = event.y;
        event.type = TOUCH_TAP;
    }

    if (pressed && wasPressed) {
        int dx = abs(event.x - pressStartX);
        int dy = abs(event.y - pressStartY);
        if (millis() - pressStartTime > TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_HOLD;
        }
    }

    if (!pressed && wasPressed) {
        int pressDuration = millis() - pressStartTime;
        int dx = abs(event.x - pressStartX);
        int dy = abs(event.y - pressStartY);

        if (pressDuration <= TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_TAP;
        } else if (dx > dy && dx > MOVE_THRESHOLD) {
            event.type = (event.x < pressStartX) ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
        } else if (dy > dx && dy > MOVE_THRESHOLD) {
            event.type = (event.y < pressStartY) ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;
        }
        event.x = pressStartX;
        event.y = pressStartY;
    }

    wasPressed = pressed;
    return event;
}

void TouchManager::setCalibration(int xMin, int xMax, int yMin, int yMax) {
    extern TFT_eSPI tft;
    // 使用 TFT_eSPI 的校準 API
    // tft.setTouchCalibrate(xMin, yMin, xMax, yMax);
    (void)xMin; (void)xMax; (void)yMin; (void)yMax;
}

void TouchManager::runCalibration() {
    #ifdef DEBUG_SERIAL
    extern TFT_eSPI tft;
    Serial.println("[Touch] Starting calibration...");
    uint16_t calData[5] = {0};
    tft.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 2);
    Serial.printf("[Touch] Calibration data: %d %d %d %d %d\n", calData[0], calData[1], calData[2], calData[3], calData[4]);
    Serial.println("[Touch] Calibration done");
    #endif
}