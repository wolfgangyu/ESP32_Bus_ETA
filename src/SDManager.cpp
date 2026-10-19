#include "SDManager.h"
#include "config.h"

SDManager::SDManager() {}

SDManager::~SDManager() {
    if (vspi) {
        delete vspi;
        vspi = nullptr;
    }
}

bool SDManager::begin() {
    // VSPI: SCK=18, MISO=19, MOSI=23, CS=5
    vspi = new SPIClass(VSPI);
    vspi->begin(18, 19, 23, 5);

    if (!SD.begin(5, *vspi)) {
        #ifdef DEBUG_SERIAL
        Serial.println("[SD] Mount failed");
        #endif
        mounted = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        #ifdef DEBUG_SERIAL
        Serial.println("[SD] No card attached");
        #endif
        mounted = false;
        return false;
    }

    #ifdef DEBUG_SERIAL
    Serial.printf("[SD] Card type: %s\n",
        cardType == CARD_MMC ? "MMC" : (cardType == CARD_SD ? "SDSC" : (cardType == CARD_SDHC ? "SDHC" : "UNKNOWN")));
    Serial.printf("[SD] Size: %llu MB\n", SD.cardSize() / (1024 * 1024));
    #endif

    mounted = true;
    listDir("/");
    return true;
}

void SDManager::listDir(const char* dirname, uint8_t levels) {
    if (!mounted) return;
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) {
        #ifdef DEBUG_SERIAL
        Serial.printf("[SD] Failed to open %s\n", dirname);
        #endif
        return;
    }

    File file = root.openNextFile();
    #ifdef DEBUG_SERIAL
    Serial.printf("[SD] Directory: %s\n", dirname);
    #endif
    while (file) {
        if (file.isDirectory()) {
            #ifdef DEBUG_SERIAL
            Serial.printf("  DIR : %s\n", file.name());
            #endif
            if (levels) listDir(file.path(), levels - 1);
        } else {
            #ifdef DEBUG_SERIAL
            Serial.printf("  FILE: %s  (%u bytes)\n", file.name(), file.size());
            #endif
        }
        file = root.openNextFile();
    }
}

bool SDManager::readFile(const char* path, String& out) {
    if (!mounted) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    out = file.readString();
    file.close();
    return true;
}

bool SDManager::writeFile(const char* path, const String& data) {
    if (!mounted) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    bool ok = file.print(data);
    file.close();
    return ok;
}

double SDManager::getTotalGB() const {
    if (!mounted) return 0.0;
    return (double)SD.totalBytes() / (1024.0 * 1024.0 * 1024.0);
}

double SDManager::getFreeGB() const {
    if (!mounted) return 0.0;
    // totalBytes 為整卡容量；無直接 free 查詢 API，
    // 粗估 free = total - 已佔用（若後續需要精確，可用 LFN 遍歷統計）
    // 此處回傳 total 同值作為顯示；精確 free 留待擴充
    return getTotalGB();
}

int SDManager::countPhotos() const {
    if (!mounted) return 0;
    int n = 0;
    File dir = SD.open("/photos");
    if (!dir || !dir.isDirectory()) return 0;
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory() && (String(f.name()).endsWith(".jpg") ||
                                 String(f.name()).endsWith(".jpeg") ||
                                 String(f.name()).endsWith(".JPG"))) {
            n++;
        }
        f = dir.openNextFile();
    }
    return n;
}