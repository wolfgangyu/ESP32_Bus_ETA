/*
 * 44_tfteSPI_ili9341.cpp
 *
 * 改用官方 Bodmer/TFT_eSPI 庫（萬人已驗證的 init 序列），
 * 排除「我手寫 bit-bang init 有 bug」這個變數。
 * 腳位依官方排線圖：SCK=14 MOSI=13 MISO=12 CS=15 DC=2 BL=27
 * init 前隔離觸控 CS=33 和 SD CS=5，防 MISO 被扯低
 * 填 4 色 + readPixel 讀回印出
 */
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// 觸控 & SD 的 CS，init 前先拉高隔離，防止 MISO 匯流排被扯低
#define TOUCH_CS 33
#define SD_CS  5

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== 44_tfteSPI_ili9341 ===");

    // strapping
    pinMode(12, OUTPUT); digitalWrite(12, LOW);
    // RGB LED 全 LOW
    pinMode(17, OUTPUT); digitalWrite(17, LOW);
    pinMode(4,  OUTPUT); digitalWrite(4,  LOW);
    pinMode(16, OUTPUT); digitalWrite(16, LOW);

    // 【關鍵】先隔離觸控 & SD CS
    pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
    pinMode(SD_CS,    OUTPUT); digitalWrite(SD_CS,    HIGH);
    delay(20);

    Serial.println("TFT_eSPI begin()...");
    tft.init();
    Serial.printf("tft width=%d height=%d (want 240x320 or 320x240)\n",
                  tft.width(), tft.height());

    // 填 4 色
    Serial.println("[fill] RED");
    tft.fillScreen(TFT_RED);
    delay(800);
    Serial.println("[fill] GREEN");
    tft.fillScreen(TFT_GREEN);
    delay(800);
    Serial.println("[fill] BLUE");
    tft.fillScreen(TFT_BLUE);
    delay(800);
    Serial.println("[fill] WHITE");
    tft.fillScreen(TFT_WHITE);
    delay(800);

    // 畫已知顏色像素再讀回
    Serial.println("[pixel] set 5,5=RED 6,5=GREEN 5,6=BLUE, then readPixel");
    tft.fillCircle(5, 5, 1, TFT_RED);
    tft.fillCircle(6, 5, 1, TFT_GREEN);
    tft.fillCircle(5, 6, 1, TFT_BLUE);
    delay(20);
    uint16_t p0 = tft.readPixel(5, 5);
    uint16_t p1 = tft.readPixel(6, 5);
    uint16_t p2 = tft.readPixel(5, 6);
    Serial.printf("[readPixel] 5,5=0x%04X(want F800) 6,5=0x%04X(want 07E0) 5,6=0x%04X(want 001F)\n",
                  p0, p1, p2);
    Serial.println((p0==0xF800 && p1==0x07E0 && p2==0x001F) ? "[PASS] TFT_eSPI readback OK"
                                                              : "[FAIL] readback mismatch");

    // 顯示文字
    Serial.println("[text] Hello");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("44_OK\n");
    tft.setCursor(10, 40);
    tft.printf("R=%04X G=%04X B=%04X\n", p0, p1, p2);

    delay(5000);
    Serial.println("[A] done, idle");
}

void loop() {
    static unsigned long last = 0;
    if (millis() - last > 5000) { last = millis(); Serial.println("[loop] alive"); }
    delay(100);
}
