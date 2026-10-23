/*
 * TouchManager.h - 觸控事件處理 (使用 TFT_eSPI 內建 XPT2046 支援)
 */

#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// TouchEvent 定義在類別外部，方便使用
enum TouchType {
    TOUCH_NONE = 0,
    TOUCH_TAP,
    TOUCH_HOLD,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT
};

struct TouchEvent {
    TouchType type = TOUCH_NONE;
    int x = 0;
    int y = 0;
    int z = 0;          // 壓力
    unsigned long time = 0;
};

class TouchManager {
private:
    bool initialized = false;

    // 狀態追蹤
    bool wasPressed = false;
    unsigned long pressStartTime = 0;
    int pressStartX = 0, pressStartY = 0;
    static const int TAP_THRESHOLD_MS = 300;
    static const int MOVE_THRESHOLD = 20;

public:
    TouchManager();
    ~TouchManager();

    void begin();
    TouchEvent read();

    // 校準相關
    void setCalibration(int xMin, int xMax, int yMin, int yMax);
    void runCalibration();
    bool isCalibrated() const { return initialized; }
};

#endif