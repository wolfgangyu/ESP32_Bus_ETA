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

    // getTouch 門檻 200（原 600）：端點到端點快速滑動時 Z 值爬坡中，
    // 高門檻讓 5 次取樣常只剩 1 次有效 → 位移算不出來 → 落回 tap。
    // 門檻降到 200 讓滑動全程都能抓到座標；鬼觸風險由 Slideshow 頁的
    // 「點按 = 換照片」（低代價）吸收。回傳值 n = 有效取樣次數（0-5）。
    uint16_t lx = 0, ly = 0;
    uint8_t n = tft->getTouch(&lx, &ly, 200);
    bool pressed = (n > 0);
    event.z = n;

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

    // 狀態機
    if (pressed && !wasPressed) {
        pressStartTime = millis();
        pressStartX = x;
        pressStartY = y;
    } else if (pressed) {
        // 按著期間：HOLD 偵測（起點附近按住超過 TAP_THRESHOLD_MS）
        int dx = abs(x - pressStartX);
        int dy = abs(y - pressStartY);
        if (millis() - pressStartTime > TAP_THRESHOLD_MS && dx < MOVE_THRESHOLD && dy < MOVE_THRESHOLD) {
            event.type = TOUCH_HOLD;
        }
    } else if (wasPressed) {
        // 放開事件：用「起點→放開點」位移判斷，不靠最後一幀，
        // 避免 XPT2046 放開瞬間 X 軸漂移把水平手勢算成 tap。
        int pressDuration = millis() - pressStartTime;
        int relX = lastX - pressStartX;
        int relY = lastY - pressStartY;
        int absRelX = abs(relX), absRelY = abs(relY);
        if (pressDuration <= TAP_THRESHOLD_MS && absRelX < MOVE_THRESHOLD && absRelY < MOVE_THRESHOLD) {
            event.type = TOUCH_TAP;
        } else if (absRelY > absRelX && absRelY > MOVE_THRESHOLD) {
            event.type = (relY < 0) ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN;
        } else if (absRelX > MOVE_THRESHOLD) {
            event.type = (relX < 0) ? TOUCH_SWIPE_LEFT : TOUCH_SWIPE_RIGHT;
        }
        // event.x/y 已是放開前最後位置（最後一幀）
    }
    wasPressed = pressed;
    return event;
}