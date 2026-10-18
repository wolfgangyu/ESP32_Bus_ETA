# HW-950 ESP32 開發板 — 確認硬體規格

> 專案：ESP32 公車到站提示系統
> 最後確認：2026-10-18（觸控、LED 三態、SD 卡、光感自動背光 全部跑通）
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

## 6. 更新紀錄

| 日期 | 內容 |
| --- | --- |
| 2026-10-10 | 初版，驅動 IC 推測為 ST7789，白屏未解 |
| 2026-10-17 | 依官方 spec PDF 確認驅動為 **ILI9341**、背光 27、init 需隔離觸控/SD CS；TFT_eSPI 跑通 |
| 2026-10-18 | v0.2.0：啟用觸控、LED 三態、SD 卡、光感自動背光 |
