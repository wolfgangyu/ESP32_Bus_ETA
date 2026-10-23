# HW-950 ESP32 開發板 — 確認硬體規格

> 專案：ESP32 公車到站提示系統
> 最後確認：2026-10-23（v1.0.0：觸控、LED 三態、SD 卡、光感自動背光、UI 定版 全部跑通）
> 來源：官方 spec PDF（bitbyg.dk ESP32-2432S024）＋實測驗證

---

## 1. 基本資訊

| 項目 | 內容 |
| --- | --- |
| **開發板** | HW-950，型號 ESP32-2432S024R（R = 電阻式觸控） |
| **ESP32 晶片** | ESP32-D0WD-V3 (rev v3.1)，雙核 240 MHz，Wi-Fi + BT 4.2 |
| **Flash** | 4 MB（32Mbit） |
| **RAM** | 320 KB SRAM，448 KB ROM |
| **序列埠** | `/dev/cu.usbserial-110`，CH340 USB-serial bridge，115200 bps |
| **供電** | USB-C（5V），消耗約 100 mA |

---

## 2. 顯示器（已確認 ✅）

| 項目 | 值 | 備註 |
| --- | --- | --- |
| **驅動 IC** | **ILI9341** | 官方 PDF 明訂；不是 ST7789/ST7796 |
| **解析度** | 240 × 320 | RGB 65K（16-bit 色彩） |
| **介面** | 4-wire SPI | 排線腳位見下表 |
| **色彩** | 16-bit RGB565 | |
| **顯示面積** | 48.96 × 36.72 mm（2.4" 對角） | 視角 >60° |

### TFT 排線腳位（官方確認）

| 訊號 | GPIO | 備註 |
| --- | --- | --- |
| TFT_SCLK | 14 | |
| TFT_MOSI | 13 | |
| TFT_MISO | 12 | 部分批次可能未接回 ESP32（不支援讀暫存器） |
| TFT_CS | 15 | strapping pin，boot 時需 LOW |
| TFT_DC | 2 | strapping pin，boot 時需 HIGH |
| TFT_BL | 27 | HIGH 點亮背光，LOW 熄滅（非 PWM） |
| TFT_RST | 硬體 | 不佔用 GPIO，接系統 reset |

> **關鍵 init 前置條件**：init 前必須把 **觸控 CS=33** 與 **SD/TF 卡 CS=5** 拉高隔離，
> 否則 MISO 匯流排可能被其他 SPI 裝置扯低，導致讀暫存器回 0x00 或 init 失誤。

---

## 3. 觸控與 SD 卡

| 項目 | 值 | 備註 |
| --- | --- | --- |
| 觸控 IC | XPT2046（電阻式） | CS=33, CLK=25, MOSI=32, MISO=39, IRQ=36 |
| SD/TF 卡槽 | SPI | CS=5, MOSI=23, CLK=18, MISO=19（獨立 VSPI 匯流排） |

---

## 4. 其他週邊

| 項目 | GPIO | 備註 |
| --- | --- | --- |
| RGB LED | 17(綠) / 4(紅) / 16(藍) | 實測 2026-10-17；原 spec PDF 標記與實測相反，已修正 |
| 光敏電阻（ADC） | 34 | 背面板面 |
| 喇叭/功放 | 26 | 可驅動 2.5W 喇叭 |
| DHT11（溫濕度） | 擴展 10 針 | IO35 / IO22 / IO21 |
| BOOT 鈕 | 0 | 按住接 USB 進入下載模式 |
| RST 鈕 | EN | 實體重設鈕 |

---

## 5. 跑通的韌體配置

- **驅動**：Bodmer/TFT_eSPI（`ILI9341_DRIVER=1`）
- **腳位注入**：`USER_SETUP_LOADED=1` + build_flags 定义，不改庫檔案
- **SPI 頻率**：20 MHz（先穩定，可上調 55 MHz）
- **init 前置**：觸控 CS=33、SD CS=5 拉高隔離，背光 27 HIGH
- **觸控**：XPT2046 (CS=33, CLK=25, MOSI=32, MISO=39, IRQ=36)，TFT_eSPI initTouch()
- **SD 卡**：VSPI (CS=5, MOSI=23, CLK=18, MISO=19)，SdFat/Arduino SD
- **LED**：GPIO17/4/16 PWM 四態 (紅常亮/綠快閃/綠心跳/全滅)，GPIO27 背光 PWM
- **光感**：GPIO34 ADC1_CH6，滑動平均+死區映射背光
- 正式入口：`src/main.cpp`

---

## 6. 觸控螢幕啟動流程與 Boot Loop 解決紀錄

### 6.1 觸控初始化正確順序（關鍵）

```cpp
// 1. strapping 處理
pinMode(12, OUTPUT); digitalWrite(12, LOW);

// 2. 隔離觸控 & SD CS（必須在 tft.init() 之前）
pinMode(TOUCH_CS, OUTPUT); digitalWrite(TOUCH_CS, HIGH);
pinMode(SD_CS,    OUTPUT); digitalWrite(SD_CS,    HIGH);
delay(20);

// 3. TFT 初始化
tft.init();
tft.setRotation(1);

// 4. 觸控初始化（延遲 3 秒，避免開機不穩定）
// 在 main.cpp loop() 中延遲初始化：
if (!touchInitialized && millis() - touchInitTime >= 3000) {
    touch.begin();  // XPT2046 Driver 獨立初始化
    touchInitialized = true;
}
```

### 6.2 觸控庫選擇：TFT_eSPI 內建 XPT2046（v1.0.0 定案）

**使用** TFT_eSPI 內建觸控（`TOUCH_CS=33` + `TOUCH_DRIVER=1`）：

| 項目 | 說明 |
|------|------|
| 驅動 | TFT_eSPI 內建（`-D TOUCH_CS=33`、`-D TOUCH_DRIVER=1`）|
| 腳位 | **只多一支 CS=33**；SCK/MOSI/MISO 與螢幕共用（14/13/12）|
| 校準 | `tft.setTouch(calData[5])`，5 值順序 `x0, x1, y0, y1, flags`；本板 flags=0 |
| 讀取 | `tft.getTouch(&x, &y, 600)`，回傳已校準+旋轉的邏輯座標 |

> ⚠️ 文件早期寫「觸控獨立 VSPI (CLK=25, MOSI=32, MISO=39)」是**錯的** —
> 實際上 XPT2046 與螢幕共用同一組 SPI 腳位，只是 CS 不同。

**踩過的兩個坑（v1.0.0 修正）：**

1. **手動 SPI 讀 XPT2046 暫存器不可行** —— 在這塊板永遠回 0。不可取代 `getTouch()`。
2. **SD 卡搶走 VSPI** —— TFT_eSPI 在 ESP32 預設用 VSPI 物件，而 `SDManager`
   又 `new SPIClass(VSPI)` + `begin(18,19,23,5)`，把同一個周邊重新接線到 SD 的腳位，
   於是觸控的讀取被送到 18/19/23（XPT2046 不在那裡）→ 永遠讀不到。
   **解法：`-D USE_HSPI_PORT=1`，讓 TFT+觸控走 HSPI，VSPI 完整留給 SD。**

3. **`getTouch()` 是非阻塞的**：沒人碰螢幕時本來就回 `pressed=0`，
   不能用「一直是 0」推論「觸控壞了」。要驗證必須實際按壓，
   或跑獨立測試韌體 `touch_test/`。

4. **`touch.begin()` 必須傳 tft**：`touch.begin(&tft)`。
   漏傳的話 TouchManager 拿不到 `TFT_eSPI*`，`initialized` 不會成立，
   `read()` 永遠回 `TOUCH_NONE`（點擊完全沒反應，且不會有任何錯誤訊息）。

### 6.3 Boot Loop 根因與解決

| 現象 | 根因 | 解決方案 |
|------|------|----------|
| 開機白屏重開 | `touch.begin()` 在 setup() 過早執行，觸控中斷衝突 | **延遲 3 秒** 在 loop() 中初始化 |
| WiFi 後白屏 | `WiFiClientSecure` 區域變數在 `http.end()` 前析構 | **HTTPS client 宣告為類別成員變數** (TDXClient::secureClient) |
| NTP 後崩潰 | `WiFiClientSecure` 在 WxManager HTTPS 請求中析構 | 改用 **HTTP** (open-meteo 支援) 或同樣用成員變數 |
| TDX API 崩潰 | 同 `WiFiClientSecure` 生命週期問題 | **client 宣告為 TDXClient 成員變數**，跨越 `http.begin()` ~ `http.end()` |

### 6.4 關鍵程式碼片段

**TDXClient.h - HTTPS client 成員變數**
```cpp
class TDXClient {
private:
    WiFiClientSecure secureClient;  // 成員變數，生命週期跨越 HTTP 請求
    // ...
    String getAccessToken() {
        secureClient.setInsecure();
        http.begin(secureClient, "https://tdx.transportdata.tw/...");
        // ...
        http.end();  // client 仍存活
        return accessToken;
    }
};
```

**WxManager.cpp - 改用 HTTP 避免 HTTPS 問題**
```cpp
// open-meteo 支援 HTTP
http.begin("http://api.open-meteo.com/v1/forecast?...");
http.begin("http://air-quality-api.open-meteo.com/v1/air-quality?...");
```

**main.cpp - 觸控延遲初始化**
```cpp
bool touchInitialized = false;
unsigned long touchInitTime = 0;

void loop() {
    // ...
    if (!touchInitialized && millis() - touchInitTime >= 3000) {
        touch.begin();
        touchInitialized = true;
    }
    // ...
}
```

---

## 7. 更新紀錄

| 日期 | 內容 |
| --- | --- |
| 2026-10-10 | 初版，驅動 IC 推測為 ST7789，白屏未解 |
| 2026-10-17 | 依官方 spec PDF 確認驅動為 **ILI9341**、背光 27、init 需隔離觸控/SD CS；TFT_eSPI 跑通 |
| 2026-10-18 | v0.2.0：啟用觸控、LED 三態、SD 卡、光感自動背光 |
| 2026-10-23 | **v1.0.0**：觸控改用 TFT_eSPI 內建（見下）；TFT+觸控改走 **HSPI**，VSPI 完整留給 SD 卡（原本 SD 的 `SPI.begin(18,19,23,5)` 會把觸控讀取帶到錯腳位）；UI 定版（Splash 顯示對時時間、ETA 4 筆/頁、東/西各自 8 秒翻頁、Slideshow 全螢幕、夜間 01–05 不抓 TDX）；Slideshow 改串流解碼（不再配置 200KB）|
