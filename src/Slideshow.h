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

    // Decode buffer (reuse to avoid fragmentation)
    static const size_t MAX_JPG_SIZE = 200 * 1024;  // 200 KB limit
    uint8_t* jpgBuffer = nullptr;

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

    // Get current photo data for drawing
    // Returns true if new photo loaded, false if using cached/placeholder
    bool getCurrentPhoto(uint8_t*& outBuf, size_t& outLen, int& outW, int& outH);

    // Debug
    void listPhotos();
};

#endif