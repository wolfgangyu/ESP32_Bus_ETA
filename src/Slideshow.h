/*
 * Slideshow.h - SD 卡照片輪播管理
 * 支援 JPEG 串流解碼，限制最大尺寸避免 OOM
 */

#ifndef SLIDESHOW_H
#define SLIDESHOW_H

#include <Arduino.h>
#include <SD.h>
#include <TFT_eSPI.h>

struct PhotoInfo {
    char filename[32];
    size_t fileSize = 0;
    int width = 0;
    int height = 0;
};

class Slideshow {
private:
    TFT_eSPI* tft = nullptr;
    File photoDir;
    PhotoInfo* photos = nullptr;
    int photoCount = 0;
    int currentIndex = 0;
    bool paused = false;
    unsigned long lastAdvanceMs = 0;
    static const unsigned long ADVANCE_INTERVAL_MS = 10000UL;  // 10 seconds

    // Decode 採串流解碼（TJpgDec.drawSdJpg），不需要大緩衝
    // 舊版配置 200KB 連續記憶體，WiFi 起來後必失敗

    // Filters
    static const char* VALID_EXTENSIONS[];
    static const int VALID_EXT_COUNT;

    bool isValidPhoto(const char* filename);
    bool loadPhotoInfo(const char* path, PhotoInfo& info);
    bool decodeJpegHeader(File& file, int& outW, int& outH);

public:
    Slideshow();
    ~Slideshow();

    void begin(TFT_eSPI* tftPtr);
    void end();

    // Main loop call
    void tick(unsigned long now);

    // Navigation
    void next();
    void prev();
    void togglePause() { paused = !paused; }
    bool isPaused() const { return paused; }
    int getCurrentIndex() const { return currentIndex; }
    int getPhotoCount() const { return photoCount; }
    const char* getCurrentFilename() const;

    // 直接把目前這張畫在 (x, y) 為左上角的 maxW×maxH 區域內（等比、置中）
    // 串流解碼，不需先把整個 JPEG 讀進 RAM。回 false = 沒照片／解碼失敗
    bool drawCurrent(int x, int y, int maxW, int maxH);

    // Debug
    void listPhotos();
};

#endif