/*
 * TouchManager.cpp - 觸控事件處理實作
 *
 * TFT_eSPI getTouch 回傳「已旋轉」的邏輯座標（0..width, 0..height）
 * 校準 5 值順序：x0, x1, y0, y1, flags
 *   flags: bit0=rotate(軸 swap), bit1=invert_x, bit2=invert_y
 * 這塊 XPT2046 + setRotation(1)：AX 對 X、B 對 Y，不 swap，flags=0
 */

#include "TouchManager.h"
#include "config.h"

TouchManager::TouchManager() {}

TouchManager::~TouchManager() {}

void TouchManager::begin(TFT_eSPI* tftPtr) {
    if (tftPtr) {
        tft = tftPtr;
    }
    if (tft) {
        uint16_t calData[5] = {
            (uint16_t)TOUCH_CAL_X0,
            (uint16_t)TOUCH_CAL_X1,
            (uint16_t)TOUCH_CAL_Y0,
            (uint16_t)TOUCH_CAL_Y1,
            0  // flags：不 swap、不 invert
        };
        tft->setTouch(calData);
        initialized = true;
        #ifdef DEBUG_SERIAL
        Serial.printf("[Touch] cal x0=%d x1=%d y0=%d y1=%d flags=0\n",
                      TOUCH_CAL_X0, TOUCH_CAL_X1, TOUCH_CAL_Y0, TOUCH_CAL_Y1);
        #endif
    }
    wasPressed = false;
}

TouchEvent TouchManager::read() {
    TouchEvent event;
    event.type = TOUCH_NONE;
    event.x = 0;
    event.y = 0;
    event.z = 0;
    event.time = millis();

    if (!initialized || !tft) return event;

    // getTouch 回傳邏輯座標（已校準+旋轉，0..width / 0..height），直接用
    uint16_t lx = 0, ly = 0;
    bool pressed = tft->getTouch(&lx, &ly, 600);
    int x = pressed ? lx : 0;
    int y = pressed ? ly : 0;
    // clamp 保險
    x = max(0, min((int)tft->width() - 1, x));
    y = max(0, min((int)tft->height() - 1, y));

    event.x = x;
    event.y = y;
    event.z = pressed ? 1 : 0;

    // 狀態機
    if (pressed && !wasPressed) {
        pressStartTime = millis();
        pressStartX = x;
        pressStartY = y;
    }

    if (pressed && wasPressed) {
        int dx = abs(x - pressStartX);
        int dy = abs(y - pressStartY);
        if (millis() - pressStartTime > TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_HOLD;
        }
    }

    if (!pressed && wasPressed) {
        int pressDuration = millis() - pressStartTime;
        int dx = abs(x - pressStartX);
        int dy = abs(y - pressStartY);

        if (pressDuration <= TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_TAP;
        } else if (dx > dy && dx > MOVE_THRESHOLD) {
            event.type = (x < pressStartX) ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
        } else if (dy > dx && dy > MOVE_THRESHOLD) {
            event.type = (y < pressStartY) ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;
        }

        event.x = x;
        event.y = y;
    }

    wasPressed = pressed;
    return event;
}