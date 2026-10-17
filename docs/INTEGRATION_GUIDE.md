# 接入 / 客製化指南

> 給下游開發者、系統整合者、或想客製化功能的人看
> 硬體事實看 [docs/ESP32.md](ESP32.md)；模組介面看 [docs/ARCHITECTURE.md](ARCHITECTURE.md)；本檔只講「怎麼改」
> 最後更新：2026-10-17

---

## 1. 快速開始

```bash
# 1. 安裝 PlatformIO（VS Code 擴充或 CLI）
# 2. 開啟專案
code /Users/wolfgang/Projects/ESP32_Bus_ETA

# 3. 本機參數（Wi-Fi、TDX 金鑰）填在 include/config_local.h（已 gitignored）
# 4. 燒錄與監控
pio run -e esp32dev -t upload
pio device monitor
```

**驗證**：序列埠印 `Connected, IP=...`、`[TDX] fetched, total=N`；螢幕顯示站牌車次。

---

## 2. 自訂站牌 / 顯示

### 改目標站牌

`include/config_local.h`：

```cpp
#define TARGET_STOP_WEST  "TPE12726"   // 西向 stopID
#define TARGET_STOP_EAST  "TPE12858"   // 東向 stopID
```

### 調整顯示筆數 / 重整間隔

- 重整間隔：`src/main.cpp` 的 `REFRESH_INTERVAL_S`（預設 60 秒）
- 顯示筆數：`src/main.cpp` `drawScreen()` 的迴圈上限（目前最多 12 筆）

### `BusInfo` 結構（`src/BusData.h`）
`routeName`、`destination`、`estimateTime`（顯示文字）、`timeStatus`、`estimateSecs`（秒數，排序依據）、`direction`（0=西/1=東）。加欄位：`BusData.h` 定義 → `BusData.cpp` 填入 → `main.cpp::drawScreen()` 繪製。

---

## 3. TDX API 串接

金鑰申請、端點、OAuth2 flow 詳見 **[TDX.md](TDX.md)**。天氣與空氣品質（open-meteo）整合見 **[API_INTEGRATIONS.md](API_INTEGRATIONS.md)**。韌體端由 `TDXClient::fetchBusETAByName(stopName)` 回傳原始 JSON，`BusData` 解析成 `BusInfo`。

關鍵：token 由 `TDXClient::getAccessToken()` 自動管理（過期提前 60s 重發），`main.cpp` 不需手動處理。

---

## 4. 螢幕客製化（TFT_eSPI）

**腳位走 `platformio.ini` build_flags 注入，不用改 TFT_eSPI 庫檔案、不用 User_Setup.h：**

```ini
-D USER_SETUP_LOADED=1
-D ILI9341_DRIVER=1
-D TFT_MISO=12 -D TFT_MOSI=13 -D TFT_SCLK=14
-D TFT_CS=15 -D TFT_DC=2 -D TFT_BL=27
-D SPI_FREQUENCY=20000000
```

- 版面 / 色彩：改 `src/main.cpp::drawScreen()`（全英文 ASCII，用 `TFT_eSPI` 內建色常數）
- 旋轉：`main.cpp` 的 `tft.setRotation(1)`
- 加自訂字體：`src/fonts/` 放字體 + build_flags 加 `-D LOAD_FONTX`，`tft.setFreeFont()`

> 畫面刻意用英文（只載 `LOAD_FONT2` 內建 ASCII 字型），載中文字型會佔 Flash 且易出怪相。要顯示中文需另加 `LOAD_GFXFF=1` + SMOOTH_FONT 中文字體檔。

---

## 5. 觸控整合（XPT2046，待做）

觸控獨立走 VSPI：CS=33、CLK=25、MOSI=32、MISO=39、IRQ=36。目前 `TouchManager` 未回 `build_src_filter`。整合步驟：

1. `build_flags` 加觸控庫（XPT2046_Touchscreen）
2. `src/TouchManager.cpp` 改用真實 SPI 腳位讀座標（現版用的是未啟用的 `tft.getTouch`，會編譯失敗，需改）
3. 把 `TouchManager.cpp` 加回 `platformio.ini` `build_src_filter`
4. `main.cpp` 觸控 tap → 觸發手動 `doFetchAndShow()`

---

## 6. 記憶體優化

| 技巧 | 位置 | 省下 |
| --- | --- | --- |
| `ArduinoJson` 用 `JsonDocument` 並控制容量 | `TDXClient.cpp` | 避免 heap 碎片 |
| 靜態文字用 `F()` / PROGMEM | `main.cpp` | 數 KB Flash |
| 降 `SPI_FREQUENCY`（20→較低）優先穩定 | build_flags | 少重傳 |
| 兩支 TDX JSON 不要併存太久 | `main.cpp` | 省 ~50KB heap |

全鏈路 Flash ~76%，heap 充裕（~150KB 餘裕）。

---

## 7. 常見問題

| 症狀 | 排查 |
| --- | --- |
| 螢幕全白 | init 前觸控 CS=33 / SD CS=5 有沒拉高隔離？背光 27 HIGH？ILI9341_DRIVER 對？（見 DEVELOPMENT.md） |
| Wi-Fi 連不上 | 2.4GHz 確認；`config_local.h` SSID/密碼；serial 看 `WiFi.status()` |
| TDX total=0 | 金鑰有效？stopID 對？serial 看原始回應 |
| Watchdog reset | strapping GPIO 12 有沒有開機拉低 |

---

## 8. 文件地圖

| 文件 | 職責 |
| --- | --- |
| `README.md` | 專案總覽、進度 |
| `docs/ESP32.md` | **硬體唯一事實來源** |
| `docs/ARCHITECTURE.md` | 模組介面、資料流、記憶體 |
| `TDX.md` | TDX 交通端點（端點、OAuth、金鑰）；天氣 AQI 見 `API_INTEGRATIONS.md` |
| `DEVELOPMENT.md` | 建置/燒錄/監控、FAQ |
| `CLAUDE.md` | AI 協作規則 + 腳位速查 |

**修改流程**：改文件 → 改程式 → `pio run` 通過 → 實機驗證 → 更新 README 進度條。
