/*
 * Slideshow.cpp - SD 卡照片輪播實作
 */

#include "Slideshow.h"
#include "config.h"
#include <SPI.h>
#include <TJpg_Decoder.h>   // 串流解碼 drawSdJpg()/getSdJpgSize()

const char* Slideshow::VALID_EXTENSIONS[] = {".jpg", ".jpeg", ".JPG", ".JPEG"};
const int Slideshow::VALID_EXT_COUNT = 4;

Slideshow::Slideshow() {}

Slideshow::~Slideshow() {
    end();
}

void Slideshow::begin(TFT_eSPI* tftPtr) {
    tft = tftPtr;

    // 不再預先配置 200KB JPEG 緩衝：改用 TJpgDec.drawSdJpg() 串流解碼，
    // 開機時 WiFi 已佔用大量 heap，200KB 連續記憶體必配置失敗。

    // Open /photos directory
    photoDir = SD.open("/photos");
    if (!photoDir || !photoDir.isDirectory()) {
        #ifdef DEBUG_SERIAL
        Serial.println("[Slideshow] /photos directory not found");
        #endif
        return;
    }

    // Count valid photos first
    File entry = photoDir.openNextFile();
    int count = 0;
    while (entry) {
        if (!entry.isDirectory() && isValidPhoto(entry.name())) {
            count++;
        }
        entry = photoDir.openNextFile();
    }
    photoDir.rewindDirectory();

    if (count == 0) {
        #ifdef DEBUG_SERIAL
        Serial.println("[Slideshow] No valid photos found");
        #endif
        return;
    }

    // Allocate photo info array
    photos = new PhotoInfo[count];
    photoCount = 0;

    // Load photo metadata
    entry = photoDir.openNextFile();
    while (entry && photoCount < count) {
        if (!entry.isDirectory() && isValidPhoto(entry.name())) {
            String path = "/photos/";
            path += entry.name();
            if (loadPhotoInfo(path.c_str(), photos[photoCount])) {
                photoCount++;
            }
        }
        entry = photoDir.openNextFile();
    }

    photoDir.rewindDirectory();
    currentIndex = 0;
    lastAdvanceMs = millis();

    #ifdef DEBUG_SERIAL
    Serial.printf("[Slideshow] Loaded %d photos\n", photoCount);
    listPhotos();
    #endif
}

void Slideshow::end() {
    if (photos) {
        delete[] photos;
        photos = nullptr;
    }
    photoCount = 0;
    currentIndex = 0;
    if (photoDir) {
        photoDir.close();
    }
}

bool Slideshow::isValidPhoto(const char* filename) {
    String fn = String(filename);
    fn.toLowerCase();
    for (int i = 0; i < VALID_EXT_COUNT; i++) {
        if (fn.endsWith(VALID_EXTENSIONS[i])) {
            return true;
        }
    }
    return false;
}

bool Slideshow::decodeJpegHeader(File& file, int& outW, int& outH) {
    // Read JPEG header to get dimensions
    // JPEG SOI marker: 0xFFD8
    // Look for SOF0 (0xFFC0) or SOF2 (0xFFC2) marker

    if (!file.seek(0)) return false;

    uint8_t buf[4];
    if (file.read(buf, 2) != 2) return false;
    if (buf[0] != 0xFF || buf[1] != 0xD8) return false;  // Not JPEG

    while (file.available()) {
        if (file.read(buf, 1) != 1) break;
        if (buf[0] != 0xFF) continue;

        if (file.read(buf, 1) != 1) break;
        uint8_t marker = buf[0];

        // SOF markers: 0xC0 (baseline), 0xC1, 0xC2 (progressive)
        if (marker >= 0xC0 && marker <= 0xC3 && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            // Read segment length (2 bytes, big-endian)
            if (file.read(buf, 2) != 2) break;
            uint16_t segLen = (buf[0] << 8) | buf[1];

            // Skip precision byte
            if (file.read(buf, 1) != 1) break;

            // Read height (2 bytes, big-endian)
            if (file.read(buf, 2) != 2) break;
            outH = (buf[0] << 8) | buf[1];

            // Read width (2 bytes, big-endian)
            if (file.read(buf, 2) != 2) break;
            outW = (buf[0] << 8) | buf[1];

            return true;
        } else if (marker == 0xD8 || marker == 0xD9) {
            // SOI or EOI - no length
            continue;
        } else {
            // Other markers have length field
            if (file.read(buf, 2) != 2) break;
            uint16_t segLen = (buf[0] << 8) | buf[1];
            if (segLen >= 2) {
                file.seek(file.position() + segLen - 2);
            }
        }
    }
    return false;
}

bool Slideshow::loadPhotoInfo(const char* path, PhotoInfo& info) {
    File file = SD.open(path, FILE_READ);
    if (!file) return false;

    info.fileSize = file.size();
    // 串流解碼不需要檔案大小上限（不再預載整個 JPEG 到 RAM）

    strncpy(info.filename, path, sizeof(info.filename) - 1);
    info.filename[sizeof(info.filename) - 1] = '\0';

    if (!decodeJpegHeader(file, info.width, info.height)) {
        #ifdef DEBUG_SERIAL
        Serial.printf("[Slideshow] Failed to decode header: %s\n", path);
        #endif
        file.close();
        return false;
    }

    file.close();
    return true;
}

void Slideshow::tick(unsigned long now) {
    if (photoCount == 0 || paused) return;

    if (now - lastAdvanceMs >= ADVANCE_INTERVAL_MS) {
        next();
        lastAdvanceMs = now;
    }
}

void Slideshow::next() {
    if (photoCount == 0) return;
    currentIndex = (currentIndex + 1) % photoCount;
}

void Slideshow::prev() {
    if (photoCount == 0) return;
    currentIndex = (currentIndex - 1 + photoCount) % photoCount;
}

const char* Slideshow::getCurrentFilename() const {
    if (photoCount == 0 || currentIndex < 0 || currentIndex >= photoCount) return nullptr;
    return photos[currentIndex].filename;
}

// 串流解碼繪製：邊讀 SD 邊解，不需把整個 JPEG 載入 RAM
bool Slideshow::drawCurrent(int x, int y, int maxW, int maxH) {
    if (photoCount == 0 || currentIndex < 0 || currentIndex >= photoCount) return false;
    const char* path = photos[currentIndex].filename;

    // 先讀 header 取得尺寸，決定降採樣倍率（1/1、1/2、1/4、1/8）
    uint16_t w = 0, h = 0;
    if (TJpgDec.getSdJpgSize(&w, &h, path) != JDR_OK || w == 0 || h == 0) {
        #ifdef DEBUG_SERIAL
        Serial.printf("[Slideshow] Header read failed: %s\n", path);
        #endif
        return false;
    }

    uint8_t scale = 0;   // 0=1/1, 1=1/2, 2=1/4, 3=1/8
    while (scale < 3 && (((int)w >> scale) > maxW || ((int)h >> scale) > maxH)) scale++;
    TJpgDec.setJpgScale(1 << scale);

    int dw = (int)w >> scale;
    int dh = (int)h >> scale;
    int dx = x + (maxW - dw) / 2;
    int dy = y + (maxH - dh) / 2;

    JRESULT r = TJpgDec.drawSdJpg(dx, dy, path);
    if (r != JDR_OK) {
        #ifdef DEBUG_SERIAL
        Serial.printf("[Slideshow] drawSdJpg failed (%d): %s\n", (int)r, path);
        #endif
        return false;
    }
    return true;
}

void Slideshow::listPhotos() {
    #ifdef DEBUG_SERIAL
    for (int i = 0; i < photoCount; i++) {
        Serial.printf("  [%d] %s  %dx%d  %u bytes\n",
                      i, photos[i].filename, photos[i].width, photos[i].height, photos[i].fileSize);
    }
    #endif
}