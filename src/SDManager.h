#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class SDManager {
private:
    SPIClass* vspi = nullptr;
    bool mounted = false;

public:
    SDManager();
    ~SDManager();

    bool begin();
    void listDir(const char* dirname = "/", uint8_t levels = 1);
    bool readFile(const char* path, String& out);
    bool writeFile(const char* path, const String& data);
    bool isMounted() const { return mounted; }
};

#endif