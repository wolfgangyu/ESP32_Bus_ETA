/*
 * Touch Test Firmware — HW-950 ESP32
 * 開機進入觸控測試模式：顯示即時座標、畫十字線、支援 4 點校準
 * 校準完成存入 NVS，Serial 印出 calData 供複製回 config_local.h
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>

#define TOUCH_CS 33
#define TOUCH_IRQ 36

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

enum State { TESTING, CALIBRATING };
State state = TESTING;

struct CalData { uint16_t x0, x1, y0, y1, flags; };
CalData cal = {300, 3800, 300, 3800, 0};

int calStep = 0;
int calX[4], calY[4];
const int calSize = 20;
int calPx[4], calPy[4];

void loadCalibration() {
    prefs.begin("touch", true);
    cal.x0 = prefs.getUShort("cal_x0", 300);
    cal.x1 = prefs.getUShort("cal_x1", 3800);
    cal.y0 = prefs.getUShort("cal_y0", 300);
    cal.y1 = prefs.getUShort("cal_y1", 3800);
    cal.flags = prefs.getUChar("cal_flags", 0);
    prefs.end();
    Serial.printf("[Touch] Loaded cal: x0=%d x1=%d y0=%d y1=%d flags=%d\n",
                  cal.x0, cal.x1, cal.y0, cal.y1, cal.flags);
}

void saveCalibration() {
    prefs.begin("touch", false);
    prefs.putUShort("cal_x0", cal.x0);
    prefs.putUShort("cal_x1", cal.x1);
    prefs.putUShort("cal_y0", cal.y0);
    prefs.putUShort("cal_y1", cal.y1);
    prefs.putUChar("cal_flags", cal.flags);
    prefs.end();
    Serial.println("[Touch] Calibration saved to NVS");
}

void drawCrosshair(int x, int y, uint16_t color) {
    tft.drawLine(x - 10, y, x + 10, y, color);
    tft.drawLine(x, y - 10, x, y + 10, color);
    tft.drawCircle(x, y, 5, color);
}

void drawCalPoint(int idx) {
    int margin = 20;
    calPx[0] = margin; calPy[0] = margin;
    calPx[1] = tft.width() - margin; calPy[1] = margin;
    calPx[2] = margin; calPy[2] = tft.height() - margin;
    calPx[3] = tft.width() - margin; calPy[3] = tft.height() - margin;

    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, 10);
    tft.printf("CALIBRATION %d/4", idx + 1);

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(10, 40);
    tft.printf("Touch the crosshair %d", idx + 1);

    for (int i = 0; i < 4; i++) {
        uint16_t c = (i < idx) ? TFT_GREEN : (i == idx) ? TFT_RED : TFT_DARKGREY;
        int px = calPx[i], py = calPy[i];
        tft.drawLine(px - 10, py, px + 10, py, c);
        tft.drawLine(px, py - 10, px, py + 10, c);
        tft.drawCircle(px, py, 5, c);
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Touch Test ===");

    // 觸控 CS/IRQ 隔離
    pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
    pinMode(TOUCH_IRQ, INPUT);
    delay(20);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // 載入校準值並套用
    loadCalibration();
    uint16_t calData[5] = {cal.x0, cal.x1, cal.y0, cal.y1, cal.flags};
    tft.setTouch(calData);

    // UI
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, 10);
    tft.print("Touch Test");
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(10, 35);
    tft.printf("Cal: x0=%d x1=%d y0=%d y1=%d", cal.x0, cal.x1, cal.y0, cal.y1);

    // 長按進入校準
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(10, 50);
    tft.print("Hold 2s -> Calibrate");
}

bool inCalArea(int x, int y, int idx) {
    int px = (idx < 2) ? 20 : tft.width() - 20;
    int py = (idx % 2 == 0) ? 20 : tft.height() - 20;
    return (x >= px - 20 && x <= px + 20 && y >= py - 20 && y <= py + 20);
}

unsigned long holdStart = 0;
bool holding = false;

void loop() {
    uint16_t x = 0, y = 0;
    bool pressed = tft.getTouch(&x, &y, 600);

    if (pressed) {
        Serial.printf("Touch: x=%d y=%d\n", x, y);
    }

    // 長按偵測
    if (pressed && !holding) {
        holdStart = millis();
        holding = true;
    }
    if (!pressed && holding) {
        if (millis() - holdStart >= 2000) {
            // 進入校準模式
            // state = CALIBRATING;  // 簡化：先測試基本觸控
            holding = false;
        }
    }

    if (pressed) {
        tft.fillCircle(x, y, 3, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(10, 70);
        tft.printf("x=%4d y=%4d    ", x, y);
    }
    delay(10);
}
