# HW-950 ESP32 Bus ETA - 技術文件

## 專案概覽
- **開發板**: HW-950 (ESP32-D0WD-V3, 2.4" ILI9341 TFT, CH340 USB)
- **框架**: PlatformIO + Arduino (espressif32)
- **螢幕**: 320×240, ILI9341, TFT_eSPI 驅動
- **觸控**: XPT2046 (電阻式), VSPI 介面

---

## 使用的 Libraries

### 核心依賴 (platformio.ini)
```ini
lib_deps = 
    lovyan03/LovyanGFX@^1.1.0      # 圖形庫 (TFT_eSPI 內部依賴)
    bblanchon/ArduinoJson@^6.21.2  # JSON 解析 (TDX API)
    bodmer/TFT_eSPI@^2.5.43        # 主要顯示驅動
    bodmer/TJpg_Decoder@^1.0.0     # JPEG 解碼 (Slideshow)
```

### 系統內建
- `SPI` - SPI 通訊
- `WiFi` - 網路連線
- `SD` - SD 卡存取
- `HTTPClient` - HTTP 請求
- `NetworkClientSecure` - HTTPS (TDX OAuth)
- `time.h` / `configTime` - NTP 對時

---

## 硬體腳位配置

### TFT (ILI9341) - HSPI
| 功能 | GPIO | 備註 |
|------|------|------|
| SCK  | 14   | HSPI CLK |
| MOSI | 13   | HSPI MOSI |
| MISO | 12   | HSPI MISO (共用觸控) |
| CS   | 15   | TFT 片選 |
| DC   | 2    | Data/Command |
| RST  | -1   | 硬體重置 |
| BL   | 27   | 背光 PWM (共陰極) |

### 觸控 (XPT2046) - VSPI
| 功能 | GPIO | 備註 |
|------|------|------|
| SCK  | 25   | VSPI CLK |
| MOSI | 32   | VSPI MOSI |
| MISO | 39   | VSPI MISO (輸入) |
| CS   | 33   | 觸控片選 |
| IRQ  | 36   | 中斷 (輸入, 僅電平) |

### 其他
| 功能 | GPIO | 備註 |
|------|------|------|
| LED 綠 | 17 | 共陰極 PWM (呼吸燈) |
| LED 紅 | 4  | 共陰極 PWM |
| LED 藍 | 16 | 共陰極 PWM |
| LDR  | 34 | 光感 ADC (自動背光) |
| SD CS | 5 | SD 卡片選 |

---

## 顯示參數 (build_flags)
```ini
-D USER_SETUP_LOADED=1
-D ILI9341_DRIVER=1
-D TFT_MISO=12
-D TFT_MOSI=13
-D TFT_SCLK=14
-D TFT_CS=15
-D TFT_DC=2
-D TFT_RST=-1
-D TFT_BL=27
-D TFT_BACKLIGHT_ON=HIGH
-D TFT_WIDTH=240
-D TFT_HEIGHT=320
-D SPI_FREQUENCY=20000000
-D LOAD_GLCD=1
-D LOAD_FONT2=1
-D SMOOTH_FONT=1
-D LOAD_FONT4=1
```

### 字型支援
- **GLCD** (8px) - 系統資訊、小標籤
- **Font 2** (16px) - 一般文字
- **Font 4** (26px) - 大字路線、ETA、時鐘
- **Smooth Font** - 平滑字體 (Font 4+)

---

## 觸控現狀與限制

### 目前實作
- **TFT_eSPI 內建 XPT2046 支援** (`tft.getTouch()`)
- VSPI 獨立總線 (GPIO 25/32/39/33)
- 校準矩陣支援

### 關鍵問題
> **觸控完全無法偵測** - `tft.getTouch()` 永遠回傳 `pressed=0`

### 已嘗試方案
1. TFT_eSPI 內建觸控 (`tft.getTouch()`)
2. XPT2046 library 直接控制 VSPI
3. 手動 SPI 讀取 XPT2046 暫存器
4. 不同校準參數矩陣
5. 不同壓力閾值 (500, 600, 1000)

### 可能硬體原因
- IRQ (GPIO 36) 僅輸入，無法作為觸發
- VSPI MISO (GPIO 39) 僅輸入，可能與 TFT 衝突
- CS (GPIO 33) 可能未正確接線
- XPT2046 晶片未上電或接線異常

---

## 畫面架構與分頁規格 (v7 更新)

### 頁面配置 (LayoutMetrics)
```cpp
screenW = 320, screenH = 240
wxH = 40              // 天氣列高度
listTop = 40          // 公車區起始 Y
listBottom = 240      // 公車區結束 Y
rowH = 44             // 單行高度 (含間距)
dirLabelH = 20        // 方向標籤高度
visibleRows = 4       // 每頁顯示行數 (實際可見 4 行，每頁 3 筆)
maxRows = 8           // 單向最多行數
photoTop = 40         // Slideshow 相片起始 Y
photoH = 200          // Slideshow 相片高度 (全螢幕減去天氣列)
```

### 公車分頁 (v7 新規格)
- **每頁項目**: 3 筆 (西向+東向合併排序，依 ETA 排序)
- **自動翻頁**: 5 秒/頁 (從 10 秒縮短)
- **總頁數**: `(總筆數 + 2) / 3` (無條件進位)
- **頁碼顯示**: 右上角 "1/5" 格式

### 分頁動畫
- **自動翻頁間隔**: 5 秒 (原 10 秒)
- **CSS 動畫**: 10s 循環，每頁停留 ~3.3 秒
- **手動翻頁**: 點擊公車列表區循環翻頁

### Slideshow 全螢幕規格
- **相片區**: 40px~240px (200px 高度，保留天氣列)
- **無跑馬燈** (移除原有 34px 跑馬燈)
- **相片最大化**: 置中、保持比例縮放、填滿剩餘空間
- **佔位圖**: 300×180 灰框 + "NO PHOTO" + "Put JPG on SD"

---

## 狀態機 (簡化為 3 頁)

```cpp
enum Page {
    PAGE_DASHBOARD = 0,    // Page 0: ETA (含 Splash)
    PAGE_SLIDESHOW = 1,    // Page 1: Slideshow 全螢幕
    PAGE_CALIBRATION = 2   // 校準頁 (長按天氣列進入)
};
```

### 頁面流程
```
啟動 → Splash (3秒) → Page 0 (ETA)
                          ↓ 點擊底部 / 左滑
                       Page 1 (Slideshow)
                          ↓ 任意點擊 / 右滑
                       Page 0 (ETA)
```

### 互動邏輯 (v7)

| 當前頁面 | 觸控動作 | 結果 |
|---------|---------|------|
| Splash (3秒) | 自動 | → Page 0 ETA |
| Page 0 ETA | 點擊底部區域 (y > 200) | → Page 1 Slideshow |
| Page 0 ETA | 左滑 | → Page 1 Slideshow |
| Page 0 ETA | 點擊公車列表 | 手動翻頁 (循環) |
| Page 0 ETA | 長按天氣列 (y < 40) | → Calibration 頁 |
| Page 1 Slideshow | 任意點擊 | → Page 0 ETA |
| Page 1 Slideshow | 右滑 | → Page 0 ETA |
| Calibration | 4點點擊 → Done/Save | → Page 0 ETA |

---

## Splash 畫面規格 (3秒)

```
┌─────────────────────┐
│   ESP32 Bus ETA     │  ← 18px 標題
│   HW-950  ILI9341   │  ← 10px 副標題
│                     │
│   SD Card: OK 7.4 GB │  ← 綠字 OK / 紅字 NOT FOUND
│                     │
│   FW: v0.2.0        │  ← Firmware 版本
│   Build: 2026/10/21 │  ← Build 日期
│                     │
│   Loading...        │  ← 底部提示
└─────────────────────┘
```

---

## 資料流向

```
TDX API (HTTPS) 
    → TDXClient (OAuth2 token 管理)
    → BusData (JSON 解析、排序、合併東西向)
    → UIManager (分頁、繪製)
    → TFT_eSPI (硬體輸出)
```

### TDX API
- **端點**: `/v2/Bus/EstimatedTimeOfArrival/City/Taipei`
- **篩選**: `contains(StopName/Zh_tw,'西新里(新新公園)')`
- **更新頻率**: 60 秒
- **單日呼叫限制**: 依 TDX 方案

### 氣象/AQI (open-meteo)
- **天氣**: `api.open-meteo.com`
- **空氣品質**: `air-quality-api.open-meteo.com`
- **更新頻率**: 10 分鐘
- **座標**: 固定在 Taipei (25.033, 121.565)

---

## 記憶體使用
- **RAM**: ~57.3 KB / 320 KB (17.5%)
- **Flash**: ~1.38 MB / 4 MB (34.0%)

---

## 已知問題待解決

1. **觸控完全失效** - 最優先，需硬體確認 (VSPI/IRQ 接線)
2. **NTP 偶爾同步失敗** - 需增加重試機制
3. **Slideshow JPEG 緩衝區配置失敗** - 記憶體不足
4. **公車資料偶爾為 0 筆** - TDX 回傳格式變動需防禦
5. **Splash 期間無公車資料** - 需在 Splash 結束後立即抓取

---

## UI 規範摘要 (v7)

| 項目 | 規格 |
|------|------|
| 螢幕解析度 | 320×240 (橫向) |
| 天氣列高度 | 40px |
| 公車列表區 | 40px~240px (200px) |
| 每頁筆數 | 3 筆 (西+東合併) |
| 行高 | 44px (含間距) |
| ETA 字體 | Font 4 (26px) + "min" 8px |
| 自動翻頁 | 5 秒/頁 |
| 頁碼位置 | 右上角 |
| Slideshow 相片高 | 200px (全螢幕減天氣列) |
| 無跑馬燈 | ✓ 已移除 |
| 無系統資訊頁 | ✓ 已移除 |
| 無 (i) 按鈕 | ✓ 已移除 |

---

## 開發待辦

- [ ] 修復觸控硬體問題 (最優先)
- [ ] Splash 結束後立即抓取公車資料
- [ ] NTP 同步失敗重試機制
- [ ] Slideshow JPEG 緩衝區優化
- [ ] TDX 回傳格式防禦性解析
- [ ] Calibration 頁面儲存校準參數到 NVS

---

*文件更新: 2026-10-23 (v7 UI 更新)*