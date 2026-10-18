# HW-950 周邊功能啟用設計文件

> **專案**：ESP32 公車到站提示系統
> **目標**：驅動觸控、讀卡器、光感電阻、藍牙、修正 LED 行為；功能先全部 enable，之後再設計 UI 進 1.0
> **優先級順序**：觸控 (1) → LED 行為修正 (2) → 讀卡器 (3) → 藍牙 (4) → 光感電阻 (5)
> **版本**：v0.2.0（功能完整版，UI 設計前）

---

## 1. 背景與目標

### 1.1 現況
- `main.cpp` 已整合：WiFi、TDX API 公車到站、氣象/AQI、NTP、TFT 顯示
- 觸控 `TouchManager`、顯示 `DisplayManager` 已有程式碼但**未編入**（`build_src_filter` 未包含）
- 硬體腳位確認完畢（詳見 `docs/ESP32.md`）
- Init 順序極度敏感：`GPIO12 LOW` → 隔離 Touch CS=33、SD CS=5 → `tft.init()`，順序不可改

### 1.2 目標
- 將 5 項硬體功能全部啟用並穩定跑在同一支韌體上
- 為後續 UI 設計（v1.0）鋪設完整的輸入/輸出管道
- 保持現有功能（公車、氣象、NTP）不受影響

---

## 2. 硬體腳位對照表（唯一事實來源：`docs/ESP32.md`）

| 功能 | IC/裝置 | GPIO | 介面 | 備註 |
|------|---------|------|------|------|
| **TFT** | ILI9341 | 14/13/12/15/2/27 | HSPI | 已跑通，build_flags 注入 |
| **觸控** | XPT2046 | 33/25/32/39/36 | SPI (共用 HSPI) | CS=33，**init 前必拉高隔離** |
| **SD 卡** | — | 5/23/18/19 | VSPI | CS=5，**init 前必拉高隔離** |
| **LED 綠** | — | 17 | PWM (ledc ch0) | 共陰極，目前已驅動 |
| **LED 紅** | — | 4 | GPIO | 共陰極，**新增驅動** |
| **LED 藍** | — | 16 | GPIO | 共陰極，保留未用 |
| **光感電阻** | LDR | 34 | ADC1_CH6 | 背面板面，讀取映射到背光 |
| **喇叭/功放** | — | 26 | DAC/I2S | 保留給藍牙/MP3 播放 |
| **DHT11** | — | 35/22/21 | GPIO | 擴展 10 針，暫不啟用 |

> ⚠️ **關鍵限制**：觸控 (XPT2046) 與 TFT 共用 HSPI 匯流排；SD 卡使用獨立 VSPI。觸控讀取時需確保 TFT 不在傳輸，或使用 `TFT_eSPI` 內建的 `getTouch()`（它會自動處理 SPI 切換）。

---

## 3. 各功能技術方案

### 3.1 觸控 (XPT2046) — 優先級 1

**現有程式碼**：`TouchManager.h/.cpp` 已實作基本事件（Tap、Hold、Swipe 四向）、校準介面。

**缺口**：
- `begin()` 只是設旗標，未真正初始化 XPT2046
- `read()` 依賴 `TFT_eSPI::getTouch()`，但 TFT_eSPI 需在 `User_Setup.h` 或 build_flags 宣告觸控腳位

**實作步驟**：
1. 在 `platformio.ini` 的 `build_flags` 加入觸控腳位定義：
   ```ini
   -D TOUCH_CS=33
   -D TOUCH_SCLK=25
   -D TOUCH_MOSI=32
   -D TOUCH_MISO=39
   -D TOUCH_IRQ=36
   -D SPI_TOUCH_FREQUENCY=2500000
   ```
2. 修改 `TouchManager::begin()`：呼叫 `tft->initTouch()`（TFT_eSPI 內建）並驗證回傳值
3. 加入預設校準值（或開機跑一次校準流程），避免座標偏移
4. 將 `TouchManager.cpp` 加入 `build_src_filter`
5. 在 `main.cpp` 的 `setup()` 中 `touch.begin(&tft)`（在 `tft.init()` **之後**）

**驗收**：
- 序列埠印出觸控座標，對應螢幕實際位置
- Tap/Hold/Swipe 事件正確觸發

---

### 3.2 LED 行為修正 — 優先級 2

**現狀**：`main.cpp` 的 `ledBegin()` 只設定 GPIO17 (綠) PWM；GPIO4 (紅)、GPIO16 (藍) 只設 `OUTPUT` + `LOW`。

**目標行為**：
| 狀態 | 紅燈 (GPIO4) | 綠燈 (GPIO17) | 藍燈 (GPIO16) |
|------|-------------|--------------|--------------|
| WiFi 未連線 / 連線失敗 | **常亮** | 熄滅 | 熄滅 |
| WiFi 連線中 | 熄滅 | **快閃 (0.5s)** | 熄滅 |
| WiFi 已連線 (正常) | 熄滅 | **1.5 秒閃一次 (心跳)** | 熄滅 |

**實作步驟**：
1. `ledBegin()`：三顆 LED 都用 `ledc` PWM（或紅/藍用單純 GPIO），統一用 `ledc` 方便亮度控制
2. 新增 `LED_RED=4`, `LED_BLUE=16` 對應的 ledc channel
3. 修改 `ledTick()` 狀態機：
   - 新增 `LED_HEARTBEAT` 狀態（1.5s 週期，亮 100ms）
   - `LED_FAST` 維持 0.5s 快閃
   - `LED_RED_ON` 常亮
4. 在 `loop()` 的 WiFi 狀態判斷邏輯對應切換 `ledState`

**注意**：共陰極，**HIGH=亮、LOW=滅**；PWM duty 越大越亮。

---

### 3.3 讀卡器 (SD Card) — 優先級 3

**硬體**：VSPI 獨立匯流排，CS=5, MOSI=23, CLK=18, MISO=19

**實作步驟**：
1. 新增 `SDManager.h/.cpp`（或直接在 `main.cpp` 內輕量封裝）
2. `SDManager::begin()`：
   ```cpp
   SPIClass vspi(VSPI);
   vspi.begin(18, 19, 23, 5);  // SCK, MISO, MOSI, CS
   if (!SD.begin(5, vspi)) { /* error */ }
   ```
3. 實作基本 API：`listDir(path)`, `readFile(path)`, `writeFile(path, data)`
4. 建議資料夾結構（預留給未來 mp3/照片/字型）：
   ```
   /sd/
     mp3/       # 放置 mp3 檔
     img/       # 放置 slideshow 圖片
     font/      # 放置中文字型
   ```
5. 開機時掃描 SD 卡是否存在，序列埠印出根目錄清單

**驗收**：
- 序列埠顯示 SD 卡容量、根目錄檔案列表
- 能讀取測試檔案內容

---

### 3.4 藍牙 (Bluetooth A2DP) — 優先級 4

**考量**：
- ESP32 經典藍牙 + BLE 共用天線與 WiFi，**共存會影響 WiFi 吞吐/穩定度**
- A2DP Sink（接收手機音訊播放）較常用；A2DP Source（傳送到藍牙喇叭）較複雜
- 記憶體佔用大（~50KB+），需注意 heap

**方案選擇**：
- **第一階段**：啟用 **BLE** 做簡易配對/狀態回報（低功耗、不影響 WiFi）
- **第二階段**：若確有「連藍牙喇叭播放 mp3」需求，再加入 `ESP32-A2DP` 庫做 A2DP Source

**實作步驟 (第一階段 BLE)**：
1. 新增 `BluetoothManager.h/.cpp`
2. 使用 `BLEDevice` (Arduino-ESP32 內建) 建立 GATT Service
3. 定義 Service UUID、Characteristic UUID（可自訂或用標準）
4. 提供：裝置名稱設定、連線狀態回報、簡易指令接收（如「重抓公車」、「切換顯示模式」）
5. 加入 `build_src_filter`

**驗收**：
- 手機 nRF Connect 掃描到裝置、可連線、可讀寫 Characteristic
- WiFi 連線品質無明顯惡化（RSSI、斷線率觀察 24h）

---

### 3.5 光感電阻 (LDR) 自動背光 — 優先級 5

**硬體**：GPIO34 (ADC1_CH6)，背面板面

**實作步驟**：
1. 新增 `LightSensor.h/.cpp`
2. `begin()`：`adcAttachPin(34)`，設定衰減 `ADC_11db` (0-3.3V)
3. `readLux()`：讀取原始 ADC 值 (0-4095) → 經驗公式或查表轉換為大致 lux
4. `mapToBacklight(lux)`：將 lux 映射到 PWM duty (0-255)
   - 建議：`duty = map(constrain(lux, 10, 2000), 10, 2000, 30, 255)`
   - 加入 **滑動平均** (5~10 次) 避免亮度跳變
5. 更新週期：每 30 秒調整一次（`LDR_REFRESH_MS = 30000`）
6. 在 `main.cpp` `loop()` 中呼叫，將 duty 寫入 `ledcWrite(BL_CHANNEL, duty)` (TFT_BL=27 已在 ledc ch0? 需確認 channel 分配)

**注意**：GPIO27 (TFT_BL) 目前在 `build_flags` 設為 `TFT_BACKLIGHT_ON=HIGH`，若要 PWM 調光，**需確認 TFT_eSPI 是否佔用同一 ledc channel**。目前 `main.cpp` `ledBegin()` 用 channel 0 控制綠燈，**背光另開一個 channel (如 ch1)**。

---

## 4. 檔案異動清單

| 檔案 | 變更類型 | 說明 |
|------|----------|------|
| `platformio.ini` | 修改 | 加入觸控 build_flags、SD/藍牙/光感相關 lib_deps、擴充 `build_src_filter` |
| `src/main.cpp` | 修改 | 整合 5 項功能初始化、loop 輪詢、LED 狀態機重寫、背光 PWM channel 分離 |
| `src/TouchManager.cpp` | 修改 | 完善 `begin()`、加入預設校準值 |
| `src/TouchManager.h` | 修改 | 如需新增介面 |
| `src/SDManager.h/.cpp` | **新增** | SD 卡讀寫封裝 |
| `src/BluetoothManager.h/.cpp` | **新增** | BLE GATT Service 封裝 |
| `src/LightSensor.h/.cpp` | **新增** | LDR 讀取 + 背光映射 |
| `include/config.h` | 修改 | 新增相關常數定義（LDR 閾值、藍牙裝置名稱等） |

---

## 5. 編譯與燒錄流程（不變）

```bash
# 建置
~/.platformio/penv/bin/pio run -e esp32dev

# 燒錄（先釋放序列埠）
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload

# 監控
pio device monitor
```

> 燒錄後**必須按板子實體 RST 鈕**才會真正執行（esp32dev boot 行為）。

---

## 6. 風險與緩解

| 風險 | 影響 | 緩解策略 |
|------|------|----------|
| 觸控 SPI 與 TFT 共用 HSPI 衝突 | 觸控讀不到、螢幕花屏 | 使用 `TFT_eSPI::getTouch()` 內建切換；若不穩定改用獨立 SPI bit-bang |
| 藍牙佔用記憶體導致 WiFi 不穩 | 現場斷線、公車資料抓不到 | 先只做 BLE，A2DP 留待驗證後再決定；監控 `ESP.getFreeHeap()` |
| Init 順序改動導致白屏 | 完全無法顯示 | **嚴守現有 init 順序**，新功能一律在 `tft.init()` 之後初始化 |
| SD 卡讀取阻塞 loop | 畫面更新卡頓 | 使用非阻塞輪詢，或放在獨立 task (xTaskCreate) |
| LDR 讀取雜訊導致背光閃爍 | 使用者體驗差 | 滑動平均 + 死區 |

---

## 7. 里程碑與驗收標準

| 階段 | 交付物 | 驗收標準 |
|------|--------|----------|
| **M1: 觸控 + LED** | 觸控座標正確、LED 三態正確 | 手指觸控序列埠印座標；斷網紅燈、連網綠燈 1.5s 心跳 |
| **M2: SD 卡** | 掛載成功、列出根目錄 | 序列埠印出檔案清單、能讀取測試 txt |
| **M3: 藍牙 BLE** | 手機可連線、收發指令 | nRF Connect 連線成功、送指令裝置有回應 |
| **M4: 光感自動背光** | 環境亮暗時背光自動調整 | 手電筒照/遮蔽感測器，背光 2-3 秒內平滑變化 |
| **M5: 整合測試** | 5 項功能同跑 24h 無崩潰 | 記憶體穩定、WiFi 不斷線、公車/氣象正常更新 |

---

## 8. 後續：UI 設計 (v1.0)

功能全部 enable 後，才進入 UI 設計階段，考慮：
- 觸控交互：頁面切換、設定選單、手動重抓
- 字型：SD 卡載入中文字型（需評估 TFT_eSPI 載入 FreeType 或 bitmap 字型）
- 版面：公車資訊、氣象、時鐘、狀態列的資訊架構重組

---

## 9. 附錄：關鍵程式碼片段參考

### 9.1 Touch build_flags (加入 platformio.ini)
```ini
-D TOUCH_CS=33
-D TOUCH_SCLK=25
-D TOUCH_MOSI=32
-D TOUCH_MISO=39
-D TOUCH_IRQ=36
-D SPI_TOUCH_FREQUENCY=2500000
```

### 9.2 LED 三態機 (main.cpp 片段)
```cpp
enum LedState { LED_RED_ON, LED_FAST, LED_HEARTBEAT, LED_OFF };

void ledTick(unsigned long now) {
    static unsigned long last = 0;
    if (now - last < 20) return;
    last = now;

    switch (ledState) {
        case LED_RED_ON:
            ledcWrite(RED_CH, 255); ledcWrite(GREEN_CH, 0); ledcWrite(BLUE_CH, 0);
            break;
        case LED_FAST:   // 0.5s 週期
            // ... 現有 cosTab 邏輯套用到綠燈 channel
            break;
        case LED_HEARTBEAT: // 1.5s 週期，亮 100ms
            static uint32_t hbPhase = 0;
            hbPhase = (hbPhase + 20) % 1500;
            ledcWrite(GREEN_CH, (hbPhase < 100) ? 40 : 0);
            ledcWrite(RED_CH, 0); ledcWrite(BLUE_CH, 0);
            break;
        case LED_OFF:
        default:
            ledcWrite(RED_CH, 0); ledcWrite(GREEN_CH, 0); ledcWrite(BLUE_CH, 0);
    }
}
```

### 9.3 LDR 背光映射 (LightSensor.cpp 片段)
```cpp
uint8_t LightSensor::mapToBacklight(int raw) {
    // raw: 0-4095 (ADC 12-bit, 11db attenuation)
    // 經驗值：室內 100-500、室外直射 2000+
    int lux = raw; // 簡化：直接用 raw 做相對映射
    lux = constrain(lux, 50, 3000);
    // 死區：避免微小變化觸發 PWM 更新
    static int lastDuty = 128;
    int duty = map(lux, 50, 3000, 30, 255);
    if (abs(duty - lastDuty) < 5) return lastDuty;
    lastDuty = duty;
    return duty;
}
```

---

**文件狀態**：草稿待確認
**下一步**：確認無誤後 → `superpowers:writing-plans` 產出詳細實作計劃 → 開始編碼