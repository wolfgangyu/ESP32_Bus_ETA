/*
 * TouchManager.h - 觸控事件處理
 */

#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

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
    TFT_eSPI* tft = nullptr;
    bool initialized = false;

    // 觸控校準參數
    int calX0 = 0, calY0 = 0, calX1 = 0, calY1 = 0;

    // 狀態追蹤
    bool wasPressed = false;
    unsigned long pressStartTime = 0;
    int pressStartX = 0, pressStartY = 0;
    static const int TAP_THRESHOLD_MS = 300;
    static const int MOVE_THRESHOLD = 20;

    TouchEvent processTouch(bool pressed, int x, int y, int z);

public:
    TouchManager();
    ~TouchManager();

    void begin(TFT_eSPI* tftPtr = nullptr);
    TouchEvent read();

    // 校準相關
    void setCalibration(int x0, int y0, int x1, int y1);
    void runCalibration();
    bool isCalibrated() const { return calX0 != 0 || calY0 != 0 || calX1 != 0 || calY1 != 0; }
};

#endif