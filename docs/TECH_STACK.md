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

## 觸控現狀（已修復 2026-10-23）

### 目前實作
- **TFT_eSPI 內建 XPT2046 支援**：`tft.setTouch(calData)` + `tft.getTouch(&x, &y, 600)`
- 校準 5 值順序：`x0, x1, y0, y1, flags`；本板 `flags = 0`（不 swap、不 invert）
- 校準值放在 `include/config_local.h`：

```c
#define TOUCH_CAL_X0  300
#define TOUCH_CAL_X1  3800
#define TOUCH_CAL_Y0  300
#define TOUCH_CAL_Y1  3800
```

### 踩過的坑（勿重蹈）
手動 SPI 讀 XPT2046 暫存器（`readTouchRegister()`）在此板上**永遠回 0**，不可行。
先前「觸控失效」其實有兩個獨立原因：

1. **程式面**：改成手動 SPI 版本後取不到值 → 已還原成 TFT_eSPI `getTouch()`
2. **驗證面**：`getTouch()` 是非阻塞的，**沒人碰螢幕時本來就回 `pressed=0`**。
   要驗證必須真的按下去，看是否變 `pressed=1` 並回合理座標

### 驗證方法
用 `touch_test/`（獨立韌體）開機即時印座標。實測按壓回報範圍合理：

| 位置 | 回報座標 |
|------|----------|
| 中央 | x≈142, y≈128 |
| 左上 | x≈9-17, y≈9-21 |
| 右上 | x≈281-285, y≈10-12 |
| 下方 | x≈13-265, y≈210-215 |

### 若日後仍抓不到觸控，依序檢查
- IRQ (GPIO 36) 僅輸入，不可當觸發源
- VSPI MISO (GPIO 39) 僅輸入，確認沒被其他裝置拉低
- TOUCH_CS (GPIO 33) 是否確實接通
- 用 `touch_test/` 重新跑一次 4 點校準，把新值寫回 `config_local.h`

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

### 公車分頁（v7.1 規格，已實作並驗證）
- **每頁項目**：3 筆（`ITEMS_PER_PAGE = 3`）
- **自動翻頁**：**每 8 秒**切下一頁（`BUS_PAGE_MS = 8000`）
- **觸發條件**：**東向、西向各自獨立判斷**，該方向**筆數 > 3 才啟用**
  - `≤ 3 筆`：固定在該方向第 1 頁，不翻頁
  - `> 3 筆`：`頁數 = ceil(筆數 / 3)`，8 秒循環換頁
- **兩向互不干擾**：可能出現西向在第 2 頁、東向還在第 1 頁
- **頁碼顯示**：`W:x/y  E:x/y`，僅在任一方向頁數 > 1 時顯示
- **實測 log**：

```
[UI] EAST page -> 2/2      ← 東向 >3 筆，翻頁
（西向 ≤3 筆，無翻頁 log）  ← 條件判斷正確
```

### 分頁計時器歸屬（重要）
翻頁節奏由 **`UIManager::updateBusScroll()`** 統一管理，`main.cpp` 每輪都必須呼叫
`ui.updateBusList()`，**不能**寫成 `if (ui.busListDirty) ui.updateBusList()` —
後者會讓計時器永遠不被檢查，頁面就不會自動翻。

### 夜間暫停 TDX（01:00–05:00）
| 時間（Asia/Taipei） | 行為 |
|---|---|
| 01:00–05:00 | **不呼叫 TDX API**；畫面顯示 `NIGHT PAUSE` + `resume 05:00`；停止翻頁；保留最後一次資料 |
| 其他時間 | 正常每 60 秒輪詢 |

- 判斷式：`tm_hour >= 1 && tm_hour < 5`
- **NTP 未對時時 `getLocalTime()` 回 false → 視為非夜間，照常抓資料**（安全預設）

### Slideshow（v7.1）
- **全螢幕**：`0..320 × 0..240`，**不保留天氣列、無跑馬燈**
- 相片等比縮放置中，`maxW = screenW-4`、`maxH = screenH-4`
- 無相片時顯示 `NO PHOTO` / `Put JPG on SD` 佔位框

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

## 記憶體使用（v7.1 實測）

- **RAM**: ~57.3 KB / 320 KB (17.5%)
- **Flash**: ~1.38 MB / 4 MB (34.0%)

---

## 已知問題

1. **Slideshow JPEG 緩衝區配置失敗** — `[Slideshow] Failed to allocate JPEG buffer`，
   開機時配置失敗（記憶體不足）；相片輪播目前顯示 `NO PHOTO` 佔位框
2. **NTP 偶爾同步失敗** — 開機時 `[NTP] sync failed`，之後每小時會重試
3. **公車資料偶爾為 0 筆** — TDX 回傳格式變動，需防禦性解析
4. **夜間暫停期間畫面停在舊資料** — 設計如此（保留最後一次資料），若想要清空需再改

### 已解決
- ~~觸控完全失效~~ → 還原 TFT_eSPI `getTouch()` + 校準值，實測正常
- ~~ETA 不會自動翻頁~~ → `updateBusList()` 改為每輪呼叫
- ~~Slideshow 保留天氣列/跑馬燈~~ → 改為全螢幕

---

## UI 規範摘要 (v7.1)

| 項目 | 規格 |
|------|------|
| 螢幕解析度 | 320×240 (橫向) |
| Splash | 3 秒，SD 狀態 + FW/Build 版本 |
| 天氣列高度 | 40px |
| 天氣列左側 | 圖標 22×22、溫度 18px、短名+AQI+RH、L/H |
| 天氣列右側 | 時鐘 24px @ y=2、日期 8px @ y=24 |
| 公車列表區 | 40px~240px (200px)，左右各 160px |
| 每頁筆數 | 3 筆（東/西**各自**分頁） |
| 行高 | 44px (含間距) |
| 路線／ETA 字體 | Font 4 (26px) + `min` 8px |
| 自動翻頁 | 8 秒/頁，**該方向 >3 筆才啟用** |
| 頁碼位置 | 右上角 `W:x/y E:x/y` |
| Slideshow | **全螢幕 320×240**，無天氣列、無跑馬燈 |
| 夜間暫停 | 01:00–05:00 不抓 TDX，顯示 `NIGHT PAUSE` |
| 無系統資訊頁 / 無 (i) 按鈕 | ✓ 已移除 |

---

## 開發待辦

- [ ] 修 Slideshow JPEG 緩衝區配置失敗（可考慮降低 `JPG_SCALE` 或縮小緩衝）
- [ ] NTP 同步失敗重試機制
- [ ] TDX 回傳格式防禦性解析
- [ ] 把 `touch_test/` 的 4 點校準結果寫回 NVS，取代 `config_local.h` 寫死值

---

*文件更新: 2026-10-23 (v7.1：觸控修復、8 秒分頁、夜間暫停、全螢幕 Slideshow)*