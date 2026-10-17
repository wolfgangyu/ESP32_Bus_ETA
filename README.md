# ESP32 公車到站提示系統

> 以 **HW-950 開發板** (ESP32-D0WD-V3, 2.4" ILI9341 TFT) 顯示 **台北市南港路三段新新公園站** 即時公車到站資訊

---

## 專案簡介

| 項目 | 內容 |
| --- | --- |
| **目標** | 開機顯示 IP 位址；顯示新新公園站公車即時到站 (TDX API) |
| **開發板** | HW-950，型號 ESP32-2432S024R（[sconline.com.tw](https://www.sconline.com.tw/tw/product/show.php?num=34727&page=1&kind1=&kw=TFT)） |
| **核心晶片** | ESP32-D0WD-V3 (rev v3.1), 240 MHz, 4 MB Flash, 320 KB SRAM |
| **顯示器** | 2.4" TFT，**ILI9341**，240×320，4-wire SPI（官方 spec 確認） |
| **觸控** | XPT2046 電阻式（CS=33），獨立 VSPI 匯流排（暫未整合） |
| **通訊** | Wi-Fi 2.4GHz, 藍牙 4.2, USB-C (CH340 燒錄) |

---

## 文件導覽（各有一份事實，避免重覆）

| 文件 | 唯一職責 | 不寫什麼 |
| --- | --- | --- |
| 本檔 `README.md` | 專案總覽、進度、文件地圖 | 不寫腳位細節、不寫 API 端點 |
| `docs/ESP32.md` | **硬體唯一事實來源**（晶片、腳位、init 前置） | 不寫韌體邏輯 |
| `docs/ARCHITECTURE.md` | 模組分工、資料流、記憶體預算 | 硬體腳位轉引 ESP32.md |
| `TDX.md` | TDX API 端點、OAuth、实测範例 | 韌體實作轉引 ARCHITECTURE |
| `DEVELOPMENT.md` | 建置/燒錄/監控指令、FAQ | 不寫架構 |
| `CLAUDE.md` | AI 協作者規則 + 腳位速查 | 詳盡規格轉引 ESP32.md |

---

## 硬體規格（已確認 ✅）

依官方 spec PDF（bitbyg.dk）與實測，**完整腳位表見 [docs/ESP32.md](docs/ESP32.md)**。速查：

| 訊號 | GPIO |
| --- | --- |
| TFT_SCLK / MOSI / MISO | 14 / 13 / 12 |
| TFT_CS / TFT_DC | 15 / 2 |
| **TFT_BL**（背光） | **27**（HIGH 點亮） |
| 觸控 CS（XPT2046） | 33 |
| SD/TF CS | 5 |
| RGB LED (G/B/R) | 17 / 4 / 16 |

> **關鍵 init 前置**：TFT init 前必須把**觸控 CS=33** 與 **SD CS=5** 拉高隔離，否則 MISO 匯流排被扯低會全白。

---

## 函式庫

| 函式庫 | 用途 | 版本 |
| --- | --- | --- |
| **Bodmer/TFT_eSPI** | ILI9341 螢幕驅動 | ^2.5.43 |
| **bblanchon/ArduinoJson** | TDX API JSON 解析 | ^6.21.2 |

> 腳位由 `platformio.ini` build_flags 注入（`USER_SETUP_LOADED=1`、`ILI9341_DRIVER=1`、`TFT_BL=27`），**不需改 TFT_eSPI 庫檔案**。

---

## 快速開始

```bash
# 1. 開啟專案 (VS Code + PlatformIO 擴充)
code /Users/wolfgang/Projects/ESP32_Bus_ETA

# 2. 本機參數（Wi-Fi、TDX 金鑰）已就緒：include/config_local.h (gitignored)

# 3. 燒錄與監控
pio run -e esp32dev -t upload
pio device monitor
```

---

## 目前進度

```text
██████████ 100%  螢幕跑通（ILI9341 + TFT_eSPI + init 隔離觸控/SD CS）
██████████ 100%  Wi-Fi 連線（No7）
████████░░  90%  TDX 拉數 + 上屏（新新公園東西向，已顯示車次）
████░░░░░░  50%  觸控整合（XPT2046 待做）
```

**里程碑：**

- 2026-10-04 專案初始化（PlatformIO + 模組骨架）
- 2026-10-17 螢幕正式跑通；改用官方 TFT_eSPI + ILI9341，確認背光 27、init 需隔離觸控/SD CS
- 2026-10-17 TDX 東西向到站資料上屏成功，專案檔案清理

---

## 授權

MIT License — 歡迎自由使用、修改、散佈。
