# HW-950 周邊功能啟用實作計劃 (v0.2.0)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 啟用觸控(XPT2046)、修正 LED 三態(紅常亮/綠快閃/綠1.5s心跳)、SD 卡讀卡器、光感電阻自動背光；藍牙暫緩。

**Architecture:** 現有 `main.cpp` 為單一整合入口，採線性資料流。新功能以 Manager 類別封裝，`main.cpp` 在 `setup()`/`loop()` 中初始化並輪詢。所有硬體 init 嚴守既有順序：GPIO12 LOW → 隔離 Touch/SD CS → `tft.init()` → 新功能 init。

**Tech Stack:** PlatformIO + Arduino (espressif32)、Bodmer/TFT_eSPI (ILI9341)、ArduinoJson、SPI/SD/ADC/LEDc 內建庫

**Spec:** `docs/superpowers/specs/2026-10-18-hw950-peripheral-enablement-design.md`

---

## Global Constraints

- **Init 順序不可改**：`pinMode(12, OUTPUT); digitalWrite(12, LOW);` → Touch CS=33 HIGH + SD CS=5 HIGH → `delay(20)` → `tft.init()` → **之後**才能初始化觸控/SD/光感/LED PWM
- **腳位由 `platformio.ini` build_flags 注入**，不建 `User_Setup.h`、不改 TFT_eSPI 庫
- **共陰極 LED**：HIGH=亮、LOW=滅；PWM duty 越大越亮
- **序列埠**：`/dev/cu.usbserial-110` 115200；燒錄後必須按實體 RST 鈕
- **中文回覆、繁體、半形空格分隔英數**
- **現成庫優先、YAGNI、第一性原理除錯**
- **FW_VERSION**：目前 `v0.1.0`，此版完成後改 `v0.2.0`

---

## Review Focus

| 輸入/條件 | 合理期望行為 | 歸屬 Task |
|-----------|-------------|-----------|
| 觸控座標與螢幕像素不對應 | 需校準或預設校準值，Tap 位置視覺一致 | Task 1 |
| WiFi 斷線瞬間 → 紅燈常亮 | 紅燈在 200ms 內亮起，不閃爍 | Task 2 |
| SD 卡未插入/壞掉 | 不 crash、序列埠印錯誤、主程式繼續跑 | Task 3 |
| 手電筒照射/遮蔽光感 | 背光在 2-3 秒內平滑過渡，無閃爍 | Task 4 |
| 所有功能同跑 24h | Heap 穩定、WiFi 不因新功能斷線、公車/氣象正常更新 | Task 5 (整合測試) |

---

## File Structure Map

| 檔案 | 責任 | 變更 |
|------|------|------|
| `platformio.ini` | 建置設定 | 新增觸控 build_flags、SD/ADC 相關 lib_deps、擴充 `build_src_filter` |
| `include/config.h` | 常數定義 | 新增 LED channel、LDR 參數、觸控校準預設值 |
| `src/main.cpp` | 主程式整合 | 重寫 LED 狀態機、整合 4 新 Manager 的 init/loop、背光 PWM channel 分離 |
| `src/TouchManager.cpp/.h` | 觸控事件 | 完善 `begin()` 呼叫 `tft->initTouch()`、加入預設校準值 |
| `src/SDManager.h/.cpp` | **新增** SD 卡封裝 | VSPI 掛載、根目錄列表、讀寫基本 API |
| `src/LightSensor.h/.cpp` | **新增** 光感+背光映射 | ADC 讀取、滑動平均、映射到背光 PWM |
| `src/LEDManager.h/.cpp` | **新增** LED 統一管理 | 三燈 PWM、四態狀態機、非阻塞 tick |

> 注：原設計文件建議把 LED 邏輯留在 `main.cpp`，但為了職責分離與測試性，**新增 `LEDManager`**，`main.cpp` 只持有實例並呼叫 `tick()`。

---

## 實作計劃

### Task 1: 觸控功能啟用 (XPT2046)

**Files:**
- Modify: `platformio.ini:26-44` (build_flags 加入觸控腳位)
- Modify: `include/config.h` (新增觸控校準預設值)
- Modify: `src/TouchManager.cpp:15-21` (完善 `begin()`)
- Modify: `src/TouchManager.h:50` (如需新增介面)
- Modify: `platformio.ini:45-51` (build_src_filter 加入 `TouchManager.cpp`)
- Modify: `src/main.cpp:220-230` (setup() 中 `tft.init()` 後初始化觸控)

**Interfaces:**
- Consumes: `TFT_eSPI& tft` (已在 main.cpp 建立並 init)
- Produces: `TouchManager::read()` 回傳 `TouchEvent`，供後續 UI 使用

- [ ] **Step 1.1: 修改 `platformio.ini` build_flags 加入觸控定義**

```ini
# 在現有 build_flags 區塊 (約第 26-44 行) 追加：
    -D TOUCH_CS=33
    -D TOUCH_SCLK=25
    -D TOUCH_MOSI=32
    -D TOUCH_MISO=39
    -D TOUCH_IRQ=36
    -D SPI_TOUCH_FREQUENCY=2500000
```

- [ ] **Step 1.2: 修改 `include/config.h` 加入觸控校準預設值**

```cpp
// 在 REFRESH_INTERVAL 之後 (約第 43 行後) 追加：
#ifndef TOUCH_CAL_X0
#define TOUCH_CAL_X0  300
#endif
#ifndef TOUCH_CAL_Y0
#define TOUCH_CAL_Y0  300
#endif
#ifndef TOUCH_CAL_X1
#define TOUCH_CAL_X1  3800
#endif
#ifndef TOUCH_CAL_Y1
#define TOUCH_CAL_Y1  3800
#endif
```

- [ ] **Step 1.3: 修改 `src/TouchManager.cpp:15-21` 完善 `begin()`**

```cpp
void TouchManager::begin(TFT_eSPI* tftPtr) {
    if (tftPtr) {
        tft = tftPtr;
    }
    // 呼叫 TFT_eSPI 內建觸控初始化，回傳 true 代表成功
    if (tft) {
        initialized = tft->initTouch();
        #ifdef DEBUG_SERIAL
        Serial.printf("[Touch] initTouch() = %s\n", initialized ? "OK" : "FAIL");
        #endif
    }
    // 套用預設校準值 (來自 config.h)
    if (initialized) {
        setCalibration(TOUCH_CAL_X0, TOUCH_CAL_Y0, TOUCH_CAL_X1, TOUCH_CAL_Y1);
    }
    wasPressed = false;
}
```

- [ ] **Step 1.4: 修改 `platformio.ini` build_src_filter 加入 TouchManager**

```ini
build_src_filter =
    +<main.cpp>
    +<WiFiManager.cpp>
    +<TDXClient.cpp>
    +<BusData.cpp>
    +<WxManager.cpp>
    +<TouchManager.cpp>   # 新增
```

- [ ] **Step 1.5: 修改 `src/main.cpp` setup() 初始化觸控**

```cpp
// 在 tft.init() 之後 (約第 250 行後) 加入：
touch.begin(&tft);
```

- [ ] **Step 1.6: 建置驗證**

```bash
~/.platformio/penv/bin/pio run -e esp32dev
# 預期：編譯通過、無警告
```

- [ ] **Step 1.7: 燒錄實測觸控座標**

```bash
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload
pio device monitor
# 觸摸螢幕四角與中心，觀察序列埠輸出座標是否對應視覺位置
# 若偏移大，調整 config.h 的 TOUCH_CAL_X0/Y0/X1/Y1 重燒
```

- [ ] **Step 1.8: Commit**

```bash
git add platformio.ini include/config.h src/TouchManager.cpp src/main.cpp
git commit -m "feat(touch): enable XPT2046 touch with default calibration"
```

---

### Task 2: LED 三態狀態機重寫 (紅常亮/綠快閃/綠1.5s心跳)

**Files:**
- Create: `src/LEDManager.h`
- Create: `src/LEDManager.cpp`
- Modify: `platformio.ini:45-51` (build_src_filter 加入 `LEDManager.cpp`)
- Modify: `include/config.h` (新增 LED channel 定義)
- Modify: `src/main.cpp:55-85` (移除舊 ledBegin/ledTick，改用 LEDManager)
- Modify: `src/main.cpp:320-335` (loop() 中 WiFi 狀態判斷改呼叫 LEDManager)

**Interfaces:**
- Consumes: 無 (純硬體控制)
- Produces: `LEDManager::begin()`, `LEDManager::tick(now)`, `LEDManager::setState(state)`

- [ ] **Step 2.1: 建立 `src/LEDManager.h`**

```cpp
#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

enum LedState {
    LED_RED_ON,      // 紅燈常亮：無網路
    LED_FAST,        // 綠燈快閃：連線中 (0.5s 週期)
    LED_HEARTBEAT,   // 綠燈心跳：已連線 (1.5s 週期，亮 100ms)
    LED_OFF          // 全熄
};

class LEDManager {
private:
    static const uint8_t CH_RED   = 0;
    static const uint8_t CH_GREEN = 1;
    static const uint8_t CH_BLUE  = 2;
    static const uint8_t CH_BL    = 3;   // 背光專用

    static const uint8_t PIN_RED   = 4;
    static const uint8_t PIN_GREEN = 17;
    static const uint8_t PIN_BLUE  = 16;
    static const uint8_t PIN_BL    = 27;

    static const uint32_t PWM_FREQ = 5000;
    static const uint8_t PWM_RES   = 8;

    LedState currentState = LED_OFF;
    unsigned long lastTick = 0;
    unsigned int phase = 0;

    // 方波/呼吸波查表 (沿用 main.cpp 既有 cosTab)
    static const unsigned char cosTab[128];

public:
    LEDManager();
    ~LEDManager();

    void begin();
    void tick(unsigned long now);
    void setState(LedState state);
    LedState getState() const { return currentState; }
};

#endif
```

- [ ] **Step 2.2: 建立 `src/LEDManager.cpp`**

```cpp
#include "LEDManager.h"

const unsigned char LEDManager::cosTab[128] = {
    0,7,14,22,29,36,44,51,59,66,73,80,88,95,102,109,115,122,129,135,142,148,154,160,166,172,177,182,187,192,196,201,
    205,208,212,215,218,221,224,226,228,231,233,234,236,237,238,239,240,241,241,242,242,243,243,243,243,243,243,243,
    242,242,241,241,240,239,238,237,236,234,233,231,228,226,224,221,218,215,212,208,205,201,196,192,187,182,177,172,
    166,160,154,148,142,135,129,122,115,109,102,95,88,80,73,66,59,51,44,36,29,22,14,7,0
};

LEDManager::LEDManager() {}
LEDManager::~LEDManager() {}

void LEDManager::begin() {
    // 紅燈
    ledcSetup(CH_RED, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_RED, CH_RED);
    // 綠燈
    ledcSetup(CH_GREEN, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_GREEN, CH_GREEN);
    // 藍燈
    ledcSetup(CH_BLUE, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_BLUE, CH_BLUE);
    // 背光
    ledcSetup(CH_BL, PWM_FREQ, PWM_RES);
    ledcAttachPin(PIN_BL, CH_BL);
    // 預設全滅
    ledcWrite(CH_RED, 0);
    ledcWrite(CH_GREEN, 0);
    ledcWrite(CH_BLUE, 0);
    ledcWrite(CH_BL, 255);  // 背光全亮
}

void LEDManager::setState(LedState state) {
    if (state != currentState) {
        currentState = state;
        phase = 0;
        lastTick = 0;
    }
}

void LEDManager::tick(unsigned long now) {
    if (now - lastTick < 20) return;
    lastTick = now;

    switch (currentState) {
        case LED_RED_ON:
            ledcWrite(CH_RED, 255);
            ledcWrite(CH_GREEN, 0);
            ledcWrite(CH_BLUE, 0);
            break;

        case LED_FAST: {  // 0.5s 週期方波
            static const unsigned long PERIOD = 500;
            phase = (phase + (unsigned int)(PERIOD / 256)) & 0xFF;
            unsigned int v = (phase < 128) ? 40 : 0;
            ledcWrite(CH_GREEN, v);
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_BLUE, 0);
            break;
        }

        case LED_HEARTBEAT: {  // 1.5s 週期，亮 100ms
            static uint32_t hbPhase = 0;
            hbPhase = (hbPhase + 20) % 1500;
            ledcWrite(CH_GREEN, (hbPhase < 100) ? 40 : 0);
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_BLUE, 0);
            break;
        }

        case LED_OFF:
        default:
            ledcWrite(CH_RED, 0);
            ledcWrite(CH_GREEN, 0);
            ledcWrite(CH_BLUE, 0);
            break;
    }
    // 背光由 LightSensor 另行控制 (CH_BL)，此處不動
}
```

- [ ] **Step 2.3: 修改 `include/config.h` 加入 LED channel 定義 (供 LightSensor 參考)**

```cpp
// 在 TIMEZONE_OFFSET 之後 (約第 62 行後) 追加：
#define LED_CH_RED   0
#define LED_CH_GREEN 1
#define LED_CH_BLUE  2
#define LED_CH_BL    3
```

- [ ] **Step 2.4: 修改 `platformio.ini` build_src_filter 加入 LEDManager**

```ini
build_src_filter =
    +<main.cpp>
    +<WiFiManager.cpp>
    +<TDXClient.cpp>
    +<BusData.cpp>
    +<WxManager.cpp>
    +<TouchManager.cpp>
    +<LEDManager.cpp>      # 新增
    +<SDManager.cpp>       # 新增 (Task 3)
    +<LightSensor.cpp>     # 新增 (Task 4)
```

- [ ] **Step 2.5: 修改 `src/main.cpp` 移除舊 LED 邏輯、改用 LEDManager**

```cpp
// 刪除第 55-85 行 (ledBegin、ledTick、cosTab 等)
// 刪除第 56-58 行 (static LedState ledState...)
// 在全域變數區 (約第 45 行後) 加入：
LEDManager led;

// 在 setup() 第 241 行 ledBegin() 替換為：
led.begin();
// 連線中：
led.setState(LED_FAST);
// 連線成功 (第 310 行 ledState = LED_SLOW 替換為)：
led.setState(LED_HEARTBEAT);
// 連線失敗 (第 317 行 ledState = LED_OFF 替換為)：
led.setState(LED_RED_ON);
```

- [ ] **Step 2.6: 修改 `src/main.cpp` loop() WiFi 狀態判斷**

```cpp
// 第 327-332 行 替換為：
if (!wifi.isConnected()) {
    led.setState(LED_RED_ON);
} else {
    led.setState(LED_HEARTBEAT);
}

// 第 324 行 ledTick(millis()) 替換為：
led.tick(millis());
```

- [ ] **Step 2.7: 建置驗證**

```bash
~/.platformio/penv/bin/pio run -e esp32dev
```

- [ ] **Step 2.8: 燒錄實測三態**

```bash
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload
pio device monitor
# 觀察：開機連線中綠燈快閃 → 連上 1.5s 心跳 → 拔網路線紅燈常亮
```

- [ ] **Step 2.9: Commit**

```bash
git add src/LEDManager.h src/LEDManager.cpp platformio.ini include/config.h src/main.cpp
git commit -m "feat(led): LEDManager with 4 states (red solid / green fast / green heartbeat / off)"
```

---

### Task 3: SD 卡讀卡器 (VSPI)

**Files:**
- Create: `src/SDManager.h`
- Create: `src/SDManager.cpp`
- Modify: `platformio.ini:45-51` (build_src_filter 已在 Task 2 加入)
- Modify: `src/main.cpp:45-50` (全域加入 SDManager 實例)
- Modify: `src/main.cpp:305-310` (setup() WiFi 連線後初始化 SD 卡)

**Interfaces:**
- Consumes: 無 (獨立 VSPI)
- Produces: `SDManager::begin() -> bool`, `SDManager::listDir(path)`, `SDManager::readFile(path)`

- [ ] **Step 3.1: 建立 `src/SDManager.h`**

```cpp
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
```

- [ ] **Step 3.2: 建立 `src/SDManager.cpp`**

```cpp
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
```

- [ ] **Step 3.3: 修改 `src/main.cpp` 加入 SDManager 實例並初始化**

```cpp
// 第 45 行附近 (全域變數區) 加入：
SDManager sd;

// 在 setup() WiFi 連線成功後、tdx.begin() 之前 (約第 305 行) 加入：
if (sd.begin()) {
    Serial.println("[SD] Ready");
} else {
    Serial.println("[SD] Not available (continue without SD)");
}
```

- [ ] **Step 3.4: 建置驗證**

```bash
~/.platformio/penv/bin/pio run -e esp32dev
```

- [ ] **Step 3.5: 燒錄實測 (插入 SD 卡)**

```bash
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload
pio device monitor
# 觀察序列埠：SD 卡類型、容量、根目錄檔案列表
```

- [ ] **Step 3.6: Commit**

```bash
git add src/SDManager.h src/SDManager.cpp src/main.cpp
git commit -m "feat(sd): SDManager with VSPI mount and basic read/write"
```

---

### Task 4: 光感電阻自動背光 (GPIO34 ADC → GPIO27 PWM)

**Files:**
- Create: `src/LightSensor.h`
- Create: `src/LightSensor.cpp`
- Modify: `include/config.h` (新增 LDR 參數)
- Modify: `src/main.cpp:45-50` (全域加入 LightSensor 實例)
- Modify: `src/main.cpp:305-310` (setup() 初始化 LightSensor)
- Modify: `src/main.cpp:350-355` (loop() 定期呼叫 LightSensor 更新背光)

**Interfaces:**
- Consumes: `LEDManager` 的背光 channel (CH_BL = 3, GPIO27)
- Produces: `LightSensor::begin()`, `LightSensor::tick(now)` 內部寫入 `ledcWrite(CH_BL, duty)`

- [ ] **Step 4.1: 修改 `include/config.h` 加入 LDR 參數**

```cpp
// 在 LED_CH_BL 之後 (約第 66 行後) 追加：
#define LDR_PIN            34
#define LDR_ADC_ATTEN      ADC_11db
#define LDR_REFRESH_MS     30000UL   // 30 秒更新一次
#define LDR_RAW_MIN        50        // 暗環境 ADC 下限
#define LDR_RAW_MAX        3000      // 亮環境 ADC 上限
#define LDR_DUTY_MIN       30        // 最暗背光 duty (0-255)
#define LDR_DUTY_MAX       255       // 最亮背光 duty
#define LDR_DEADZONE       5         // 死區，避免抖動
#define LDR_SMOOTH_N       8         // 滑動平均樣本數
```

- [ ] **Step 4.2: 建立 `src/LightSensor.h`**

```cpp
#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>

class LightSensor {
private:
    static const uint8_t PIN = LDR_PIN;
    static const uint8_t CH_BL = LED_CH_BL;  // 背光 PWM channel

    int samples[LDR_SMOOTH_N] = {0};
    int sampleIdx = 0;
    int sampleCount = 0;
    int lastDuty = 128;
    unsigned long lastUpdate = 0;

    int readRaw();
    int smooth(int raw);
    uint8_t mapToDuty(int smoothedRaw);

public:
    LightSensor();
    ~LightSensor();

    void begin();
    void tick(unsigned long now);
    int getRaw() const;
    int getSmoothed() const;
    uint8_t getDuty() const { return lastDuty; }
};

#endif
```

- [ ] **Step 4.3: 建立 `src/LightSensor.cpp`**

```cpp
#include "LightSensor.h"
#include "config.h"

LightSensor::LightSensor() {}
LightSensor::~LightSensor() {}

void LightSensor::begin() {
    adcAttachPin(PIN);
    analogSetAttenuation(LDR_ADC_ATTEN);
    // 初始讀取填滿緩衝
    for (int i = 0; i < LDR_SMOOTH_N; i++) {
        samples[i] = readRaw();
        delay(2);
    }
    sampleCount = LDR_SMOOTH_N;
    // 設定初始背光
    int smoothed = smooth(samples[0]);
    lastDuty = mapToDuty(smoothed);
    ledcWrite(CH_BL, lastDuty);
    #ifdef DEBUG_SERIAL
    Serial.printf("[LDR] init raw=%d duty=%d\n", samples[0], lastDuty);
    #endif
}

int LightSensor::readRaw() {
    return analogRead(PIN);  // 0-4095 (12-bit)
}

int LightSensor::smooth(int raw) {
    samples[sampleIdx] = raw;
    sampleIdx = (sampleIdx + 1) % LDR_SMOOTH_N;
    if (sampleCount < LDR_SMOOTH_N) sampleCount++;

    long sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += samples[i];
    return sum / sampleCount;
}

uint8_t LightSensor::mapToDuty(int smoothedRaw) {
    int constrained = constrain(smoothedRaw, LDR_RAW_MIN, LDR_RAW_MAX);
    int duty = map(constrained, LDR_RAW_MIN, LDR_RAW_MAX, LDR_DUTY_MIN, LDR_DUTY_MAX);
    // 死區：變化小於 LDR_DEADZONE 不更新
    if (abs(duty - lastDuty) < LDR_DEADZONE) return lastDuty;
    return duty;
}

void LightSensor::tick(unsigned long now) {
    if (now - lastUpdate < LDR_REFRESH_MS) return;
    lastUpdate = now;

    int raw = readRaw();
    int smoothed = smooth(raw);
    uint8_t duty = mapToDuty(smoothed);

    if (duty != lastDuty) {
        lastDuty = duty;
        ledcWrite(CH_BL, duty);
        #ifdef DEBUG_SERIAL
        Serial.printf("[LDR] raw=%d smooth=%d duty=%d\n", raw, smoothed, duty);
        #endif
    }
}

int LightSensor::getRaw() const {
    return samples[(sampleIdx - 1 + LDR_SMOOTH_N) % LDR_SMOOTH_N];
}

int LightSensor::getSmoothed() const {
    long sum = 0;
    for (int i = 0; i < sampleCount; i++) sum += samples[i];
    return sampleCount ? (sum / sampleCount) : 0;
}
```

- [ ] **Step 4.4: 修改 `src/main.cpp` 加入 LightSensor 實例並初始化/輪詢**

```cpp
// 第 45 行附近 (全域變數區) 加入：
LightSensor ldr;

// 在 setup() SD 卡初始化後 (約第 310 行) 加入：
ldr.begin();

// 在 loop() 末端 delay(50) 之前 (約第 354 行) 加入：
ldr.tick(millis());
```

- [ ] **Step 4.5: 建置驗證**

```bash
~/.platformio/penv/bin/pio run -e esp32dev
```

- [ ] **Step 4.6: 燒錄實測**

```bash
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload
pio device monitor
# 手電筒照射板背面 GPIO34 → 背光變亮
# 手遮蔽 GPIO34 → 背光變暗
# 觀察序列埠 [LDR] 輸出，確認平滑過渡、無閃爍
```

- [ ] **Step 4.7: Commit**

```bash
git add src/LightSensor.h src/LightSensor.cpp include/config.h src/main.cpp
git commit -m "feat(ldr): LightSensor auto backlight with smoothing and deadzone"
```

---

### Task 5: 整合測試、FW_VERSION 升級、文件同步

**Files:**
- Modify: `include/config.h:50-52` (FW_VERSION v0.1.0 → v0.2.0)
- Modify: `docs/ESP32.md` (更新「跑通的韌體配置」與「更新紀錄」)
- Modify: `docs/API_INTEGRATIONS.md` (如有新增 API 整合說明)

**Interfaces:** 無 (驗收與文件)

- [ ] **Step 5.1: 升級 FW_VERSION**

```cpp
// include/config.h 第 51 行：
#define FW_VERSION "v0.2.0"
```

- [ ] **Step 5.2: 更新 `docs/ESP32.md` 跑通配置與紀錄**

```markdown
## 5. 跑通的韌體配置 (更新)
- 觸控：XPT2046 (CS=33, CLK=25, MOSI=32, MISO=39, IRQ=36)，TFT_eSPI initTouch()
- SD 卡：VSPI (CS=5, MOSI=23, CLK=18, MISO=19)，SdFat/Arduino SD
- LED：GPIO17/4/16 PWM 四態 (紅常亮/綠快閃/綠心跳/全滅)，GPIO27 背光 PWM
- 光感：GPIO34 ADC1_CH6，滑動平均+死區映射背光

## 6. 更新紀錄 (新增)
| 日期 | 內容 |
| --- | --- |
| 2026-10-18 | v0.2.0：啟用觸控、LED 三態、SD 卡、光感自動背光 |
```

- [ ] **Step 5.3: 長時間穩定性測試 (建議 1h+)**

```bash
# 燒錄 v0.2.0
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload
pio device monitor
# 觀察 1h+：
# - Heap 穩定 (Serial.printf("Free heap: %d\n", ESP.getFreeHeap()) 可加在 loop 印一次)
# - WiFi 不斷線、公車 60s 更新正常、氣象 10min 更新正常
# - NTP 每小時同步正常
# - 觸控、SD、光感、LED 皆正常運作
```

- [ ] **Step 5.4: Commit & Tag**

```bash
git add include/config.h docs/ESP32.md
git commit -m "release: v0.2.0 - peripheral enablement (touch, led, sd, ldr)"
git tag v0.2.0
```

---

## 執行順序建議

```
Task 1 (觸控) → Task 2 (LED) → Task 3 (SD) → Task 4 (光感) → Task 5 (整合測試)
```

- Task 1~4 互不依賴介面，可平行開發；但建議**循序**做，每完成一個就燒錄驗證，避免一次引入太多變數難除錯
- Task 5 必須在前四項都跑通後執行

---

**Plan complete and saved to `docs/superpowers/plans/2026-10-18-hw950-peripheral-enablement.md`. Please review the plan. Does it capture what you want?**