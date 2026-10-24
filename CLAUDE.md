# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

ESP32 公車到站提示系統 — HW-950 開發板 (ESP32-D0WD-V3, 2.4" **ILI9341** TFT, CH340 USB)

## 專案目標

1. 開機顯示 IP 位址
2. 顯示台北市南港路三段新新公園站公車即時到站 (TDX API)，東西向

## 常用指令

```bash
# 建置（預設 env esp32dev，編 main.cpp + 正式模組）
~/.platformio/penv/bin/pio run -e esp32dev

# 燒錄（port 常被 serial monitor 佔住，先釋放）
kill -9 $(lsof -t /dev/cu.usbserial-110) 2>/dev/null; sleep 1
~/.platformio/penv/bin/pio run -e esp32dev -t upload

# 監控
pio device monitor
```

- 序列埠 `/dev/cu.usbserial-110`（CH340）。pio 不在 PATH，用 `~/.platformio/penv/bin/pio`。
- 無 lint / 單元測試框架；驗證靠 `pio run` 編譯 + 上機 + serial 輸出。
- 上傳後要真正執行，需按板子**實體 RST 鈕**（esp32dev boot 行為）。

## 技術棧 (Do NOT introduce others)

- PlatformIO + Arduino framework (espressif32)
- Bodmer/TFT_eSPI for **ILI9341**（腳位走 build_flags 注入，不改庫檔案）
- ArduinoJson for TDX API
- WiFi.h for network

## 跨檔案架構（big picture）

資料流是**線性串接**，`main.cpp` 是唯一的整合入口（`platformio.ini build_src_filter` 決定編入哪些）：

```text
main.cpp
  ├─ UIManager        drawPage()、updateClock()、updateBusList()、updateSlideshowPhoto()
  ├─ WiFiManager      連線 + 斷線重連（非阻塞輪詢）
  ├─ TDXClient        OAuth2 token 自動管理 + 抓東西向 ETA raw JSON
  ├─ BusData          解析 JSON → BusInfo[]，東西向 merge，estimateSecs 排序
  ├─ WxManager        open-meteo 氣象 + AQI（免費免金鑰），10min 週期
  ├─ TouchManager     TFT_eSPI 內建 XPT2046（HSPI, CS=33）
  ├─ NTP             time.nist.gov / time.stdtime.gov.tw，GMT+8 固定 offset，每小時重對
  └─ LED (GPIO17 綠) PWM 方波閃爍：連線中 0.5s 快閃 / 已連線 1s 慢閃 / 離線熄
```

**關鍵耦合點（改錯會全白 / 編不過）：**

- 腳位全由 `platformio.ini` 的 `build_flags` 注入（`USER_SETUP_LOADED=1`、`ILI9341_DRIVER=1`、`TFT_BL=27` 等）
- `main.cpp` 開機先做「strapping GPIO12 拉低 → 隔離觸控 CS=33、SD CS=5 → `tft.init()`」，順序不能改（MISO 會被扯低→全白；12 沒拉低→watchdog reset）。
- 氣象/AQI 用 open-meteo 兩個 host（`api.open-meteo.com` 天氣、`air-quality-api.open-meteo.com` AQI），坐標寫死 `WxManager.cpp`；改 10min 週期改 `main.cpp` 的 `WX_REFRESH_MS`。
- `REFRESH_INTERVAL_S=60` 是 `main.cpp` 內的 `static const int`（60s 公車輪詢）；`config.h` 的 `REFRESH_INTERVAL` 已停用。
- `src/44_tfteSPI_ili9341.cpp` 是早期的 smoke test 快照，**未**編入 `esp32dev` env，勿改、勿刪（留作 init 對照）。

## 硬體腳位 (確認済み，詳見 docs/ESP32.md)

- TFT (ILI9341)：SCK=14, MOSI=13, MISO=12, CS=15, DC=2, **BL=27**（RST 走硬體）
- 觸控 XPT2046：CS=33（已整合，TFT_eSPI 內建驅動，校準值存 config_local.h）
- SD/TF 卡：CS=5
- Strapping：GPIO **12 開機必拉低**；GPIO 15 是 strapping 但可當 TFT_CS（12/15 拉低才能開機，開機後 15 設 HIGH 作 CS idle）
- RGB LED：17(綠)/4(紅)/16(藍)；韌體目前只驅動 GPIO17 綠燈（實測 2026-10-17，共陰極：LOW=滅、HIGH=亮）
- init 順序（strapping→隔離 CS→tft.init）見架構章節，不在此重述

## 關鍵規則

- TDX API Key、Wi-Fi 密碼**只能**放在 `include/config_local.h` (gitignored)；金鑰來源備份檔 `/.env` 也已 gitignore，**兩處都**勿上傳
- `TARGET_CITY`（`include/config.h`）控制 TDX 端點城市代碼，預設 `"Taipei"`；改其他縣市時同步更新 `TARGET_STOP_NAME`
- `FW_VERSION`（`include/config.h`）出 release 前手動改成 `vX.Y.Z` 再建置，開機 serial 會印 `[info] fw=... build=...`。出 release 流程見 `DEVELOPMENT.md` 的 Release 流程章節
- Slideshow 色彩：`TJpgDec.setSwapBytes(true)` 必須開啟（tjpgd RGB565 高 byte=R，ILI9341 SPI MSB=B），改照片解碼路徑時勿忘
- 中文回覆、繁體、半形空格分隔英數
- 先用現成庫，再造輪子；Bug 從第一性原理分析

## 授權

本專案 MIT License，**不得作為商品銷售**；沈伯洋、沈伯洋台北市長競選組織、黑熊學院 三方例外。詳見 `LICENSE.md`。

## 參考

- docs/ESP32.md — 硬體唯一事實來源
- docs/ARCHITECTURE.md — 模組分工與資料流
- docs/TECH_STACK.md — 技術棧、互動邏輯、分頁規格
- LICENSE.md — 授權條款
- TDX API: <https://tdx.transportdata.tw/>
