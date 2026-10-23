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
    if (!tft) {
        // 沒拿到 tft 就沒辦法 getTouch()。靜默失敗會讓「點擊沒反應」極難查，
        // 所以這裡直接喊出來。
        Serial.println("[Touch] ERROR: begin() called without TFT_eSPI pointer - touch disabled");
        initialized = false;
        return;
    }
    {
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

    // 放開時 getTouch() 回 0,0，不能拿它算位移或當事件座標，
    // 否則每次放開都會被誤判成「滑動」且座標為 (0,0)。
    int x, y;
    if (pressed) {
        x = max(0, min((int)tft->width() - 1, (int)lx));
        y = max(0, min((int)tft->height() - 1, (int)ly));
        lastX = x;
        lastY = y;
    } else {
        x = lastX;   // 沿用最後一次按住的座標
        y = lastY;
    }

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
        // event.x/y 已是放開前最後位置
    }

    wasPressed = pressed;
    return event;
}